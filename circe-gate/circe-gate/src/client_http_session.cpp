// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_session.hpp"
#include "client_websocket_session.hpp"
#include "mmain.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/interserver_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_auth.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/utilities.hpp"
#include "singletons/ip_ban_list.hpp"
#include <poseidon/job_base.hpp>
#include <poseidon/sock_addr.hpp>
#include <poseidon/http/request_headers.hpp>
#include <poseidon/http/response_headers.hpp>
#include <poseidon/http/urlencoded.hpp>
#include <poseidon/http/exception.hpp>
#include <poseidon/websocket/handshake.hpp>
#include <poseidon/websocket/exception.hpp>
#include <poseidon/singletons/magic_daemon.hpp>
#include <poseidon/zlib.hpp>

namespace Circe {
namespace Gate {

class Client_http_session::Web_socket_handshake_job : public Poseidon::Job_base {
private:
	const Poseidon::Socket_base::Delayed_shutdown_guard m_guard;
	const boost::weak_ptr<Client_http_session> m_weak_http_session;

	Poseidon::Http::Request_headers m_req_headers;
	boost::weak_ptr<Client_web_socket_session> m_weak_ws_session;

public:
	Web_socket_handshake_job(const boost::shared_ptr<Client_http_session> &http_session, Poseidon::Http::Request_headers req_headers, const boost::shared_ptr<Client_web_socket_session> &ws_session)
		: m_guard(http_session), m_weak_http_session(http_session)
		, m_req_headers(STD_MOVE(req_headers)), m_weak_ws_session(ws_session)
	{
		//
	}

private:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_http_session;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(http_session, m_weak_http_session.lock());
		if(!http_session || http_session->has_been_shutdown_write()){
			return;
		}
		const AUTO(ws_session, m_weak_ws_session.lock());
		if(!ws_session){
			http_session->send_default_and_shutdown(Poseidon::Http::status_bad_request);
			return;
		}
		LOG_CIRCE_TRACE("Client Web_socket establishment: uri = ", m_req_headers.uri, ", headers = ", m_req_headers.headers);

		try {
			http_session->sync_decode_uri(m_req_headers.uri);
			DEBUG_THROW_ASSERT(http_session->m_decoded_uri);

			AUTO(ws_resp, Poseidon::Web_socket::make_handshake_response(m_req_headers));
			DEBUG_THROW_UNLESS(ws_resp.status_code == Poseidon::Http::status_switching_protocols, Poseidon::Http::Exception, ws_resp.status_code, STD_MOVE(ws_resp.headers));
			http_session->send(STD_MOVE(ws_resp), Poseidon::Stream_buffer());
		} catch(Poseidon::Http::Exception &e){
			LOG_CIRCE_WARNING("Http::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			http_session->send_default_and_shutdown(e.get_status_code(), e.get_headers());
			return;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			http_session->send_default_and_shutdown(Poseidon::Http::status_internal_server_error);
			return;
		}
		try {
			ws_session->sync_authenticate(http_session->m_decoded_uri.get(), m_req_headers.get_params);
			DEBUG_THROW_ASSERT(ws_session->m_auth_token);
		} catch(Poseidon::Web_socket::Exception &e){
			LOG_CIRCE_WARNING("Poseidon::Web_socket::Exception thrown: code = ", e.get_status_code(), ", what = ", e.what());
			ws_session->shutdown(e.get_status_code(), e.what());
			return;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::Web_socket::status_internal_error, e.what());
			return;
		}
	}
};

Client_http_session::Client_http_session(Poseidon::Move<Poseidon::Unique_file> socket)
	: Poseidon::Http::Session(STD_MOVE(socket))
	, m_client_uuid(Poseidon::Uuid::random())
	, m_first_request(true)
{
	LOG_CIRCE_DEBUG("Client_http_session constructor: remote = ", get_remote_info());
}
Client_http_session::~Client_http_session(){
	LOG_CIRCE_DEBUG("Client_http_session destructor: remote = ", get_remote_info());
}

void Client_http_session::sync_decode_uri(const std::string &uri){
	PROFILE_ME;

	std::string decoded_uri;
	std::istringstream iss(uri);
	Poseidon::Http::url_decode(iss, decoded_uri);
	DEBUG_THROW_UNLESS(iss && !decoded_uri.empty(), Poseidon::Http::Exception, Poseidon::Http::status_bad_request);
	LOG_CIRCE_DEBUG("Decoded URI: ", decoded_uri);
	m_decoded_uri = STD_MOVE_IDN(decoded_uri);
}
void Client_http_session::sync_authenticate(Poseidon::Http::Verb verb, const std::string &decoded_uri, const Poseidon::Option_map &params, const Poseidon::Option_map &headers)
try {
	PROFILE_ME;

	const AUTO(http_enabled, get_config<bool>("client_http_enabled", false));
	DEBUG_THROW_UNLESS(http_enabled, Poseidon::Http::Exception, Poseidon::Http::status_service_unavailable);

	boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > servers_avail;
	Auth_connector::get_all_clients(servers_avail);
	DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Http::Exception, Poseidon::Http::status_bad_gateway);
	const AUTO(auth_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(auth_conn);

	Protocol::Auth::Http_authentication_request auth_req;
	auth_req.client_uuid = m_client_uuid;
	auth_req.client_ip   = get_remote_info().ip();
	auth_req.verb        = verb;
	auth_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(auth_req.params, params);
	Protocol::copy_key_values(auth_req.headers, headers);
	LOG_CIRCE_TRACE("Sending request: ", auth_req);
	Protocol::Auth::Http_authentication_response auth_resp;
	Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
	LOG_CIRCE_TRACE("Received response: ", auth_resp);
	DEBUG_THROW_UNLESS(!auth_resp.auth_token.empty(), Poseidon::Http::Exception, boost::numeric_cast<Poseidon::Http::Status_code>(auth_resp.status_code), Protocol::copy_key_values(STD_MOVE_IDN(auth_resp.headers)));
	LOG_CIRCE_DEBUG("Auth server has allowed HTTP client: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);

	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::Rcnts::view("Connection has been shut down"));
	LOG_CIRCE_TRACE("Got authentication token: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	m_auth_token = STD_MOVE_IDN(auth_resp.auth_token);
} catch(...){
	Ip_ban_list::accumulate_auth_failure(get_remote_info().ip());
	throw;
}

Poseidon::Option_map Client_http_session::make_retry_after_headers(boost::uint64_t time_remaining) const {
	PROFILE_ME;

	Poseidon::Option_map headers;
	headers.set(Poseidon::Rcnts::view("Retry-After"), boost::lexical_cast<std::string>(time_remaining / 1000));
	return headers;
}

boost::shared_ptr<Poseidon::Http::Upgraded_session_base> Client_http_session::on_low_level_request_end(boost::uint64_t content_length, Poseidon::Option_map headers){
	PROFILE_ME;

	// Find and kill malicious clients early.
	if(m_first_request){
		const AUTO(time_remaining, Ip_ban_list::get_ban_time_remaining(get_remote_info().ip()));
		if(time_remaining != 0){
			LOG_CIRCE_WARNING("Client IP is banned: remote = ", get_remote_info(), ", time_remaining = ", time_remaining);
			Poseidon::Http::Session::send_default_and_shutdown(Poseidon::Http::status_service_unavailable, make_retry_after_headers(time_remaining));
			return VAL_INIT;
		}
		m_first_request = false;
	}

	const AUTO_REF(req_headers, Poseidon::Http::Session::get_low_level_request_headers());
	LOG_CIRCE_DEBUG("Received HTTP request from ", get_remote_info(), "\n", Poseidon::Http::get_string_from_verb(req_headers.verb), ' ', req_headers.uri, '\n', req_headers.headers);

	const AUTO_REF(upgrade_str, req_headers.headers.get("Upgrade"));
	if(::strcasecmp(upgrade_str.c_str(), "websocket") == 0){
		AUTO(ws_session, boost::make_shared<Client_web_socket_session>(virtual_shared_from_this<Client_http_session>()));
		Poseidon::enqueue(boost::make_shared<Web_socket_handshake_job>(virtual_shared_from_this<Client_http_session>(), req_headers, ws_session));
		return STD_MOVE_IDN(ws_session);
	} else if(!upgrade_str.empty()){
		LOG_CIRCE_WARNING("HTTP Upgrade header not handled: remote = ", get_remote_info(), ", upgrade_str = ", upgrade_str);
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::status_not_implemented);
	}

	const AUTO(exempt_private, get_config<bool>("client_generic_exempt_private_addresses", true));
	if(exempt_private && Poseidon::Sock_addr(get_remote_info()).is_private()){
		LOG_CIRCE_DEBUG("Client exempted: ", get_remote_info());
	} else {
		Ip_ban_list::accumulate_http_request(get_remote_info().ip());
	}

	return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
}

void Client_http_session::on_sync_expect(Poseidon::Http::Request_headers req_headers){
	PROFILE_ME;

	switch(req_headers.verb){
	case Poseidon::Http::verb_options:
		break;

	case Poseidon::Http::verb_get:
	case Poseidon::Http::verb_put:
	case Poseidon::Http::verb_delete:
	case Poseidon::Http::verb_head:
	case Poseidon::Http::verb_post:
		sync_decode_uri(req_headers.uri);
		DEBUG_THROW_ASSERT(m_decoded_uri);
		sync_authenticate(req_headers.verb, m_decoded_uri.get(), req_headers.get_params, req_headers.headers);
		DEBUG_THROW_ASSERT(m_auth_token);
		break;

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::status_not_implemented);
	}

	return Poseidon::Http::Session::on_sync_expect(STD_MOVE(req_headers));
}
void Client_http_session::on_sync_request(Poseidon::Http::Request_headers req_headers, Poseidon::Stream_buffer req_entity){
	PROFILE_ME;

	const AUTO(time_remaining, Ip_ban_list::get_ban_time_remaining(get_remote_info().ip()));
	DEBUG_THROW_UNLESS(time_remaining == 0, Poseidon::Http::Exception, Poseidon::Http::status_service_unavailable, make_retry_after_headers(time_remaining));

	const AUTO(resp_encoding_preferred, Poseidon::Http::pick_content_encoding(req_headers));
	DEBUG_THROW_UNLESS(resp_encoding_preferred != Poseidon::Http::content_encoding_not_acceptable, Poseidon::Http::Exception, Poseidon::Http::status_not_acceptable);

	Poseidon::Http::Response_headers resp_headers = { 10001 };
	Poseidon::Stream_buffer resp_entity;

	// Handle the request.
	switch(req_headers.verb){
	case Poseidon::Http::verb_options:
		resp_headers.status_code = Poseidon::Http::status_ok;
		break;

	case Poseidon::Http::verb_get:
	case Poseidon::Http::verb_put:
	case Poseidon::Http::verb_delete:
	case Poseidon::Http::verb_head:
	case Poseidon::Http::verb_post: {
		if(!m_decoded_uri){
			sync_decode_uri(req_headers.uri);
			DEBUG_THROW_ASSERT(m_decoded_uri);
		}
		if(!m_auth_token){
			sync_authenticate(req_headers.verb, m_decoded_uri.get(), req_headers.get_params, req_headers.headers);
			DEBUG_THROW_ASSERT(m_auth_token);
		}

		boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > servers_avail;
		Foyer_connector::get_all_clients(servers_avail);
		DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Http::Exception, Poseidon::Http::status_bad_gateway);
		const AUTO(foyer_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
		DEBUG_THROW_ASSERT(foyer_conn);

		Protocol::Foyer::Http_request_to_box foyer_req;
		foyer_req.client_uuid = m_client_uuid;
		foyer_req.client_ip   = get_remote_info().ip();
		foyer_req.auth_token  = m_auth_token.get();
		foyer_req.verb        = req_headers.verb;
		foyer_req.decoded_uri = m_decoded_uri.get();
		Protocol::copy_key_values(foyer_req.params, STD_MOVE(req_headers.get_params));
		Protocol::copy_key_values(foyer_req.headers, STD_MOVE(req_headers.headers));
		if((req_headers.verb == Poseidon::Http::verb_put) || (req_headers.verb == Poseidon::Http::verb_post)){
			foyer_req.entity = STD_MOVE(req_entity);
		}
		LOG_CIRCE_TRACE("Sending request: ", foyer_req);
		Protocol::Foyer::Http_response_from_box foyer_resp;
		Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
		LOG_CIRCE_TRACE("Received response: ", foyer_resp);

		resp_headers.status_code = boost::numeric_cast<Poseidon::Http::Status_code>(foyer_resp.status_code);
		resp_headers.headers     = Protocol::copy_key_values(STD_MOVE_IDN(foyer_resp.headers));
		if(req_headers.verb != Poseidon::Http::verb_head){
			resp_entity = STD_MOVE(foyer_resp.entity);
		}
		break; }

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::status_not_implemented);
	}

	// Fill in other fields of the response headers.
	const AUTO(desc, Poseidon::Http::get_status_code_desc(resp_headers.status_code));
	resp_headers.reason = desc.desc_short;
	const AUTO(keep_alive, Poseidon::Http::is_keep_alive_enabled(req_headers) && (resp_headers.status_code / 100 <= 3));
	resp_headers.headers.set(Poseidon::Rcnts::view("Connection"), keep_alive ? "Keep-Alive" : "Close");
	resp_headers.headers.set(Poseidon::Rcnts::view("Access-Control-Allow-Origin"), "*");
	resp_headers.headers.set(Poseidon::Rcnts::view("Access-Control-Allow-Headers"), "Authorization, Content-Type");

	// Add the type of contents if the response entity is not empty and no explicit type is given.
	const std::size_t size_original = resp_entity.size();
	if((size_original != 0) && !resp_headers.headers.has("Content-Type")){
		const char *const content_type = Poseidon::Magic_daemon::guess_mime_type(resp_entity.squash(), resp_entity.size());
		LOG_CIRCE_TRACE("Guessed Content-Type: ", content_type);
		resp_headers.headers.set(Poseidon::Rcnts::view("Content-Type"), content_type);
	}

	// Select an encoding for compression if the response entity is not empty and no explicit encoding is given.
	Poseidon::Http::Content_encoding resp_encoding;
	if(size_original == 0){
		resp_encoding = Poseidon::Http::content_encoding_identity;
	} else if(resp_headers.headers.has("Content-Encoding")){
		resp_encoding = Poseidon::Http::content_encoding_identity;
	} else if(resp_encoding_preferred == Poseidon::Http::content_encoding_gzip){
		resp_encoding = Poseidon::Http::content_encoding_gzip;
	} else if(resp_encoding_preferred == Poseidon::Http::content_encoding_deflate){
		resp_encoding = Poseidon::Http::content_encoding_deflate;
	} else {
		resp_encoding = Poseidon::Http::content_encoding_identity;
	}
	// Compress it if possible.
	switch(resp_encoding){
	case Poseidon::Http::content_encoding_gzip: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using GZIP: compression_level = ", compression_level);
		Poseidon::Deflator deflator(true, compression_level);
		deflator.put(resp_entity);
		resp_entity = deflator.finalize();
		const std::size_t size_deflated = resp_entity.size();
		LOG_CIRCE_TRACE("GZIP result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0l / size_original, "%)");
		resp_headers.headers.set(Poseidon::Rcnts::view("Content-Encoding"), "gzip");
		break; }
	case Poseidon::Http::content_encoding_deflate: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using DEFLATE: compression_level = ", compression_level);
		Poseidon::Deflator deflator(false, compression_level);
		deflator.put(resp_entity);
		resp_entity = deflator.finalize();
		const std::size_t size_deflated = resp_entity.size();
		LOG_CIRCE_TRACE("DEFLATE result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0l / size_original, "%)");
		resp_headers.headers.set(Poseidon::Rcnts::view("Content-Encoding"), "deflate");
		break; }
	default:
		break;
	}
	// For 4xx and 5xx responses, create a default entity if there isn't one.
	if((resp_headers.status_code / 100 >= 4) && resp_entity.empty()){
		AUTO(pair, Poseidon::Http::make_default_response(resp_headers.status_code, STD_MOVE(resp_headers.headers)));
		resp_headers = STD_MOVE(pair.first);
		resp_entity  = STD_MOVE(pair.second);
	}
	send(STD_MOVE(resp_headers), STD_MOVE(resp_entity));

	// HTTP is stateless.
	m_decoded_uri.reset();
	m_auth_token.reset();
}

bool Client_http_session::has_been_shutdown() const NOEXCEPT {
	PROFILE_ME;

	return Poseidon::Http::Session::has_been_shutdown_write();
}
bool Client_http_session::shutdown(Poseidon::Web_socket::Status_code status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const AUTO(ws_session, boost::dynamic_pointer_cast<Client_web_socket_session>(get_upgraded_session()));
	if(ws_session){
		return ws_session->shutdown(status_code, reason);
	}
	LOG_CIRCE_DEBUG("Shutting down HTTP connection: remote = ", get_remote_info());
	shutdown_read();
	return shutdown_write();
}

bool Client_http_session::send(Poseidon::Http::Response_headers resp_headers, Poseidon::Stream_buffer entity){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending HTTP response to ", get_remote_info(), "\n", resp_headers.status_code, " ", resp_headers.reason, "\n", resp_headers.headers);
	return Poseidon::Http::Session::send(STD_MOVE(resp_headers), STD_MOVE(entity));
}
bool Client_http_session::send_default_and_shutdown(Poseidon::Http::Status_code status_code, const Poseidon::Option_map &headers) NOEXCEPT {
	PROFILE_ME;

	LOG_CIRCE_TRACE("Sending HTTP message: remote = ", get_remote_info(), ", status_code = ", status_code, ", headers = ", headers);
	return Poseidon::Http::Session::send_default_and_shutdown(status_code, headers);
}

}
}
