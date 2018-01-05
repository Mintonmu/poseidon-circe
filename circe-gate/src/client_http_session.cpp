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
#include <poseidon/zlib.hpp>

namespace Circe {
namespace Gate {

class ClientHttpSession::WebSocketHandshakeJob : public Poseidon::JobBase {
private:
	const Poseidon::SocketBase::DelayedShutdownGuard m_guard;
	const boost::weak_ptr<ClientHttpSession> m_weak_http_session;

	Poseidon::Http::RequestHeaders m_req_headers;
	boost::weak_ptr<ClientWebSocketSession> m_weak_ws_session;

public:
	WebSocketHandshakeJob(const boost::shared_ptr<ClientHttpSession> &http_session, Poseidon::Http::RequestHeaders req_headers, const boost::shared_ptr<ClientWebSocketSession> &ws_session)
		: m_guard(http_session), m_weak_http_session(http_session)
		, m_req_headers(STD_MOVE(req_headers)), m_weak_ws_session(ws_session)
	{ }

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
			http_session->send_default_and_shutdown(Poseidon::Http::ST_BAD_REQUEST);
			return;
		}
		LOG_CIRCE_TRACE("Client WebSocket establishment: uri = ", m_req_headers.uri, ", headers = ", m_req_headers.headers);

		try {
			http_session->sync_decode_uri(m_req_headers.uri);
			DEBUG_THROW_ASSERT(http_session->m_decoded_uri);

			AUTO(ws_resp, Poseidon::WebSocket::make_handshake_response(m_req_headers));
			DEBUG_THROW_UNLESS(ws_resp.status_code == Poseidon::Http::ST_SWITCHING_PROTOCOLS, Poseidon::Http::Exception, ws_resp.status_code, STD_MOVE(ws_resp.headers));
			http_session->send(STD_MOVE(ws_resp), Poseidon::StreamBuffer());
		} catch(Poseidon::Http::Exception &e){
			LOG_CIRCE_WARNING("Http::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			http_session->send_default_and_shutdown(e.get_status_code(), e.get_headers());
			return;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			http_session->send_default_and_shutdown(Poseidon::Http::ST_INTERNAL_SERVER_ERROR);
			return;
		}
		try {
			ws_session->sync_authenticate(http_session->m_decoded_uri.get(), m_req_headers.get_params);
			DEBUG_THROW_ASSERT(ws_session->m_auth_token);
		} catch(Poseidon::WebSocket::Exception &e){
			LOG_CIRCE_WARNING("Poseidon::WebSocket::Exception thrown: code = ", e.get_status_code(), ", what = ", e.what());
			ws_session->shutdown(e.get_status_code(), e.what());
			return;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
			return;
		}
	}
};

ClientHttpSession::ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket)
	: Poseidon::Http::Session(STD_MOVE(socket))
	, m_client_uuid(Poseidon::Uuid::random())
{
	LOG_CIRCE_DEBUG("ClientHttpSession constructor: remote = ", get_remote_info());
}
ClientHttpSession::~ClientHttpSession(){
	LOG_CIRCE_DEBUG("ClientHttpSession destructor: remote = ", get_remote_info());
}

void ClientHttpSession::sync_decode_uri(const std::string &uri){
	PROFILE_ME;

	std::string decoded_uri;
	std::istringstream iss(uri);
	Poseidon::Http::url_decode(iss, decoded_uri);
	DEBUG_THROW_UNLESS(iss && !decoded_uri.empty(), Poseidon::Http::Exception, Poseidon::Http::ST_BAD_REQUEST);
	LOG_CIRCE_DEBUG("Decoded URI: ", decoded_uri);
	m_decoded_uri = STD_MOVE_IDN(decoded_uri);
}
void ClientHttpSession::sync_authenticate(Poseidon::Http::Verb verb, const std::string &decoded_uri, const Poseidon::OptionalMap &params, const Poseidon::OptionalMap &headers)
try {
	PROFILE_ME;

	const AUTO(http_enabled, get_config<bool>("client_http_enabled", false));
	DEBUG_THROW_UNLESS(http_enabled, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

	boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > servers_avail;
	AuthConnector::get_all_clients(servers_avail);
	DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_GATEWAY);
	const AUTO(auth_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(auth_conn);

	Protocol::Auth::HttpAuthenticationRequest auth_req;
	auth_req.client_uuid = m_client_uuid;
	auth_req.client_ip   = get_remote_info().ip();
	auth_req.verb        = verb;
	auth_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(auth_req.params, params);
	Protocol::copy_key_values(auth_req.headers, headers);
	LOG_CIRCE_TRACE("Sending request: ", auth_req);
	Protocol::Auth::HttpAuthenticationResponse auth_resp;
	Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
	LOG_CIRCE_TRACE("Received response: ", auth_resp);
	DEBUG_THROW_UNLESS(!auth_resp.auth_token.empty(), Poseidon::Http::Exception, boost::numeric_cast<Poseidon::Http::StatusCode>(auth_resp.status_code), Protocol::copy_key_values(STD_MOVE(auth_resp.headers)));
	LOG_CIRCE_DEBUG("Auth server has allowed HTTP client: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);

	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::sslit("Connection has been shut down"));
	LOG_CIRCE_TRACE("Got authentication token: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	m_auth_token = STD_MOVE_IDN(auth_resp.auth_token);
} catch(...){
	IpBanList::accumulate_auth_failure(get_remote_info().ip());
	throw;
}

Poseidon::OptionalMap ClientHttpSession::make_retry_after_headers(boost::uint64_t time_remaining) const {
	PROFILE_ME;

	Poseidon::OptionalMap headers;
	headers.set(Poseidon::sslit("Retry-After"), boost::lexical_cast<std::string>(time_remaining / 1000));
	return headers;
}

boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> ClientHttpSession::on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers){
	PROFILE_ME;

	const AUTO(time_remaining, IpBanList::get_ban_time_remaining(get_remote_info().ip()));
	if(time_remaining != 0){
		LOG_CIRCE_WARNING("Client IP is banned: remote = ", get_remote_info(), ", time_remaining = ", time_remaining);
		Poseidon::Http::Session::send_default_and_shutdown(Poseidon::Http::ST_SERVICE_UNAVAILABLE, make_retry_after_headers(time_remaining));
		return VAL_INIT;
	}

	const AUTO_REF(req_headers, Poseidon::Http::Session::get_low_level_request_headers());
	LOG_CIRCE_DEBUG("Client HTTP request:\n", Poseidon::Http::get_string_from_verb(req_headers.verb), ' ', req_headers.uri, '\n', req_headers.headers);

	const AUTO_REF(upgrade_str, req_headers.headers.get("Upgrade"));
	if(::strcasecmp(upgrade_str.c_str(), "websocket") == 0){
		AUTO(ws_session, boost::make_shared<ClientWebSocketSession>(virtual_shared_from_this<ClientHttpSession>()));
		Poseidon::enqueue(boost::make_shared<WebSocketHandshakeJob>(virtual_shared_from_this<ClientHttpSession>(), req_headers, ws_session));
		return STD_MOVE_IDN(ws_session);
	} else if(!upgrade_str.empty()){
		LOG_CIRCE_WARNING("HTTP Upgrade header not handled: remote = ", get_remote_info(), ", upgrade_str = ", upgrade_str);
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}

	const AUTO(exempt_private, get_config<bool>("client_generic_exempt_private_addresses", true));
	if(exempt_private && Poseidon::SockAddr(get_remote_info()).is_private()){
		LOG_CIRCE_DEBUG("Client exempted: ", get_remote_info());
	} else {
		IpBanList::accumulate_http_request(get_remote_info().ip());
	}

	return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
}

void ClientHttpSession::on_sync_expect(Poseidon::Http::RequestHeaders req_headers){
	PROFILE_ME;

	switch(req_headers.verb){
	case Poseidon::Http::V_OPTIONS:
		break;

	case Poseidon::Http::V_GET:
	case Poseidon::Http::V_PUT:
	case Poseidon::Http::V_DELETE:
	case Poseidon::Http::V_HEAD:
	case Poseidon::Http::V_POST:
		sync_decode_uri(req_headers.uri);
		DEBUG_THROW_ASSERT(m_decoded_uri);
		sync_authenticate(req_headers.verb, m_decoded_uri.get(), req_headers.get_params, req_headers.headers);
		DEBUG_THROW_ASSERT(m_auth_token);
		break;

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}

	return Poseidon::Http::Session::on_sync_expect(STD_MOVE(req_headers));
}
void ClientHttpSession::on_sync_request(Poseidon::Http::RequestHeaders req_headers, Poseidon::StreamBuffer req_entity){
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Received HTTP message: remote = ", get_remote_info(), ", verb = ", Poseidon::Http::get_string_from_verb(req_headers.verb), ", headers = ", req_headers.headers, ", req_entity.size() = ", req_entity.size());

	const AUTO(resp_encoding_preferred, Poseidon::Http::pick_content_encoding(req_headers));
	DEBUG_THROW_UNLESS(resp_encoding_preferred != Poseidon::Http::CE_NOT_ACCEPTABLE, Poseidon::Http::Exception, Poseidon::Http::ST_NOT_ACCEPTABLE);

	Poseidon::Http::ResponseHeaders resp_headers = { 10001 };
	Poseidon::StreamBuffer resp_entity;

	// Handle the request.
	switch(req_headers.verb){
	case Poseidon::Http::V_OPTIONS:
		resp_headers.status_code = Poseidon::Http::ST_OK;
		break;

	case Poseidon::Http::V_GET:
	case Poseidon::Http::V_PUT:
	case Poseidon::Http::V_DELETE:
	case Poseidon::Http::V_HEAD:
	case Poseidon::Http::V_POST: {
		if(!m_decoded_uri){
			sync_decode_uri(req_headers.uri);
			DEBUG_THROW_ASSERT(m_decoded_uri);
		}
		if(!m_auth_token){
			sync_authenticate(req_headers.verb, m_decoded_uri.get(), req_headers.get_params, req_headers.headers);
			DEBUG_THROW_ASSERT(m_auth_token);
		}

		boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > servers_avail;
		FoyerConnector::get_all_clients(servers_avail);
		DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_GATEWAY);
		const AUTO(foyer_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
		DEBUG_THROW_ASSERT(foyer_conn);

		Protocol::Foyer::HttpRequestToBox foyer_req;
		foyer_req.client_uuid = m_client_uuid;
		foyer_req.client_ip   = get_remote_info().ip();
		foyer_req.auth_token  = m_auth_token.get();
		foyer_req.verb        = req_headers.verb;
		foyer_req.decoded_uri = m_decoded_uri.get();
		Protocol::copy_key_values(foyer_req.params, STD_MOVE(req_headers.get_params));
		Protocol::copy_key_values(foyer_req.headers, STD_MOVE(req_headers.headers));
		if((req_headers.verb == Poseidon::Http::V_PUT) || (req_headers.verb == Poseidon::Http::V_POST)){
			foyer_req.entity = STD_MOVE(req_entity);
		}
		LOG_CIRCE_TRACE("Sending request: ", foyer_req);
		Protocol::Foyer::HttpResponseFromBox foyer_resp;
		Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
		LOG_CIRCE_TRACE("Received response: ", foyer_resp);

		resp_headers.status_code = boost::numeric_cast<Poseidon::Http::StatusCode>(foyer_resp.status_code);
		resp_headers.headers     = Protocol::copy_key_values(STD_MOVE(foyer_resp.headers));
		if(req_headers.verb != Poseidon::Http::V_HEAD){
			resp_entity = STD_MOVE(foyer_resp.entity);
		}
		break; }

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}

	// Fill in other fields of the response headers.
	const AUTO(desc, Poseidon::Http::get_status_code_desc(resp_headers.status_code));
	resp_headers.reason = desc.desc_short;
	const AUTO(keep_alive, Poseidon::Http::is_keep_alive_enabled(req_headers) && (resp_headers.status_code / 100 <= 3));
	resp_headers.headers.set(Poseidon::sslit("Connection"), keep_alive ? "Keep-Alive" : "Close");
	resp_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Origin"), "*");
	resp_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Headers"), "Authorization, Content-Type");

	// Select an encoding for compression if the response entity is not empty and no explicit encoding is given.
	Poseidon::Http::ContentEncoding resp_encoding;
	const std::size_t size_original = resp_entity.size();
	if(size_original == 0){
		resp_encoding = Poseidon::Http::CE_IDENTITY;
	} else if(resp_headers.headers.has("Content-Encoding")){
		resp_encoding = Poseidon::Http::CE_IDENTITY;
	} else if(resp_encoding_preferred == Poseidon::Http::CE_GZIP){
		resp_encoding = Poseidon::Http::CE_GZIP;
	} else if(resp_encoding_preferred == Poseidon::Http::CE_DEFLATE){
		resp_encoding = Poseidon::Http::CE_DEFLATE;
	} else {
		resp_encoding = Poseidon::Http::CE_IDENTITY;
	}
	// Compress it if possible.
	switch(resp_encoding){
	case Poseidon::Http::CE_GZIP: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using GZIP: compression_level = ", compression_level);
		Poseidon::Deflator deflator(true, compression_level);
		deflator.put(resp_entity);
		resp_entity = deflator.finalize();
		const std::size_t size_deflated = resp_entity.size();
		LOG_CIRCE_TRACE("GZIP result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0l / size_original, "%)");
		resp_headers.headers.set(Poseidon::sslit("Content-Encoding"), "gzip");
		break; }
	case Poseidon::Http::CE_DEFLATE: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using DEFLATE: compression_level = ", compression_level);
		Poseidon::Deflator deflator(false, compression_level);
		deflator.put(resp_entity);
		resp_entity = deflator.finalize();
		const std::size_t size_deflated = resp_entity.size();
		LOG_CIRCE_TRACE("DEFLATE result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0l / size_original, "%)");
		resp_headers.headers.set(Poseidon::sslit("Content-Encoding"), "deflate");
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

bool ClientHttpSession::has_been_shutdown() const NOEXCEPT {
	PROFILE_ME;

	return Poseidon::Http::Session::has_been_shutdown_write();
}
bool ClientHttpSession::shutdown(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const AUTO(ws_session, boost::dynamic_pointer_cast<ClientWebSocketSession>(get_upgraded_session()));
	if(ws_session){
		return ws_session->shutdown(status_code, reason);
	}
	LOG_CIRCE_DEBUG("Shutting down HTTP connection: remote = ", get_remote_info());
	shutdown_read();
	return shutdown_write();
}

bool ClientHttpSession::send(Poseidon::Http::ResponseHeaders resp_headers, Poseidon::StreamBuffer entity){
	PROFILE_ME;

	LOG_CIRCE_TRACE("Sending HTTP message: remote = ", get_remote_info(), ", status_code = ", resp_headers.status_code, ", headers = ", resp_headers.headers, ", entity.size() = ", entity.size());
	return Poseidon::Http::Session::send(STD_MOVE(resp_headers), STD_MOVE(entity));
}
bool ClientHttpSession::send_default_and_shutdown(Poseidon::Http::StatusCode status_code, const Poseidon::OptionalMap &headers) NOEXCEPT {
	PROFILE_ME;

	LOG_CIRCE_TRACE("Sending HTTP message: remote = ", get_remote_info(), ", status_code = ", status_code, ", headers = ", headers);
	return Poseidon::Http::Session::send_default_and_shutdown(status_code, headers);
}

}
}
