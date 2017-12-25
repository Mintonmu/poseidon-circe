// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_session.hpp"
#include "client_websocket_session.hpp"
#include "mmain.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/cbpp_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"
#include "protocol/messages_gate_foyer.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/job_base.hpp>
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

	Poseidon::Http::RequestHeaders m_request_headers;
	boost::weak_ptr<ClientWebSocketSession> m_weak_ws_session;

public:
	WebSocketHandshakeJob(const boost::shared_ptr<ClientHttpSession> &http_session, Poseidon::Http::RequestHeaders request_headers, const boost::shared_ptr<ClientWebSocketSession> &ws_session)
		: m_guard(http_session), m_weak_http_session(http_session)
		, m_request_headers(STD_MOVE(request_headers)), m_weak_ws_session(ws_session)
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
		LOG_CIRCE_TRACE("Client WebSocket establishment: uri = ", m_request_headers.uri, ", headers = ", m_request_headers.headers);

		std::string decoded_uri;
		try {
			decoded_uri = safe_decode_uri(m_request_headers.uri);
			LOG_CIRCE_TRACE("Decoded URI: ", decoded_uri);

			AUTO(ws_resp, Poseidon::WebSocket::make_handshake_response(m_request_headers));
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
			AUTO(auth_token, ws_session->sync_authenticate(decoded_uri, m_request_headers.get_params));
			LOG_CIRCE_DEBUG("Auth server has allowed WebSocket client: remote = ", ws_session->get_remote_info(), ", auth_token = ", auth_token);
			DEBUG_THROW_ASSERT(!ws_session->m_auth_token);
			ws_session->m_auth_token = STD_MOVE_IDN(auth_token);
		} catch(Poseidon::WebSocket::Exception &e){
			LOG_CIRCE_WARNING("Poseidon::WebSocket::Exception thrown: code = ", e.get_code(), ", what = ", e.what());
			ws_session->shutdown(e.get_code(), e.what());
			return;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
			return;
		}
	}
};

std::string ClientHttpSession::safe_decode_uri(const std::string &uri){
	PROFILE_ME;

	std::string decoded_uri;
	std::istringstream iss(uri);
	Poseidon::Http::url_decode(iss, decoded_uri);
	DEBUG_THROW_UNLESS(iss && !decoded_uri.empty(), Poseidon::Http::Exception, Poseidon::Http::ST_BAD_REQUEST);
	LOG_CIRCE_TRACE("Decoded URI: ", decoded_uri);
	return decoded_uri;
}

ClientHttpSession::ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket)
	: Poseidon::Http::Session(STD_MOVE(socket))
	, m_session_uuid(Poseidon::Uuid::random())
{
	LOG_CIRCE_DEBUG("ClientHttpSession constructor: remote = ", get_remote_info());
}
ClientHttpSession::~ClientHttpSession(){
	LOG_CIRCE_DEBUG("ClientHttpSession destructor: remote = ", get_remote_info());
}

std::string ClientHttpSession::sync_authenticate(Poseidon::Http::Verb verb, const std::string &decoded_uri, const Poseidon::OptionalMap &params, const Poseidon::OptionalMap &headers){
	PROFILE_ME;

	const AUTO(http_enabled, get_config<bool>("client_http_enabled", false));
	DEBUG_THROW_UNLESS(http_enabled, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

	const AUTO(auth_conn, AuthConnector::get_connection());
	DEBUG_THROW_UNLESS(auth_conn, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_GATEWAY);

	Protocol::AG_HttpAuthenticationResponse auth_resp;
	{
		Protocol::GA_HttpAuthenticationRequest auth_req;
		auth_req.client_uuid = get_session_uuid();
		auth_req.client_ip   = get_remote_info().ip();
		auth_req.verb        = verb;
		auth_req.decoded_uri = decoded_uri;
		Protocol::copy_key_values(auth_req.params, params);
		Protocol::copy_key_values(auth_req.headers, headers);
		LOG_CIRCE_TRACE("Sending request: ", auth_req);
		Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
		LOG_CIRCE_TRACE("Received response: ", auth_resp);
	}
	DEBUG_THROW_UNLESS(auth_resp.status_code == 0, Poseidon::Http::Exception, auth_resp.status_code, Protocol::copy_key_values(STD_MOVE(auth_resp.headers)));

	LOG_CIRCE_DEBUG("Got authentication token: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	return STD_MOVE(auth_resp.auth_token);
}

boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> ClientHttpSession::on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers){
	PROFILE_ME;

	const AUTO_REF(request_headers, Poseidon::Http::Session::get_low_level_request_headers());
	LOG_CIRCE_TRACE("Client HTTP request: verb = ", Poseidon::Http::get_string_from_verb(request_headers.verb), ", uri = ", request_headers.uri, ", headers = ", request_headers.headers);

	const AUTO_REF(upgrade_str, request_headers.headers.get("Upgrade"));
	if(::strcasecmp(upgrade_str.c_str(), "websocket") == 0){
		AUTO(ws_session, boost::make_shared<ClientWebSocketSession>(virtual_shared_from_this<ClientHttpSession>()));
		Poseidon::enqueue(boost::make_shared<WebSocketHandshakeJob>(virtual_shared_from_this<ClientHttpSession>(), request_headers, ws_session));
		return STD_MOVE_IDN(ws_session);
	} else if(upgrade_str.empty()){
		return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
	} else {
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}
}

void ClientHttpSession::on_sync_expect(Poseidon::Http::RequestHeaders request_headers){
	PROFILE_ME;

	m_decoded_uri = safe_decode_uri(request_headers.uri);
	AUTO(auth_token, sync_authenticate(request_headers.verb, m_decoded_uri, request_headers.get_params, request_headers.headers));
	LOG_CIRCE_DEBUG("Auth server has allowed HTTP client: remote = ", get_remote_info(), ", auth_token = ", auth_token);
	DEBUG_THROW_ASSERT(!m_auth_token);
	m_auth_token = STD_MOVE_IDN(auth_token);

	return Poseidon::Http::Session::on_sync_expect(STD_MOVE(request_headers));
}
void ClientHttpSession::on_sync_request(Poseidon::Http::RequestHeaders request_headers, Poseidon::StreamBuffer request_entity){
	PROFILE_ME;

	if(m_decoded_uri.empty()){
		m_decoded_uri = safe_decode_uri(request_headers.uri);
	}
	if(!m_auth_token){
		AUTO(auth_token, sync_authenticate(request_headers.verb, m_decoded_uri, request_headers.get_params, request_headers.headers));
		LOG_CIRCE_DEBUG("Auth server has allowed HTTP client: remote = ", get_remote_info(), ", auth_token = ", auth_token);
		DEBUG_THROW_ASSERT(!m_auth_token);
		m_auth_token = STD_MOVE_IDN(auth_token);
	}

	const AUTO(response_encoding_preferred, Poseidon::Http::pick_content_encoding(request_headers));
	DEBUG_THROW_UNLESS(response_encoding_preferred != Poseidon::Http::CE_NOT_ACCEPTABLE, Poseidon::Http::Exception, Poseidon::Http::ST_NOT_ACCEPTABLE);

	Poseidon::Http::ResponseHeaders response_headers = { 10001 };
	std::basic_string<unsigned char> response_entity_as_string;

	// Process the request. `HEAD` is handled in the same way as `GET`.
	// Fill in `response_headers.status_code` and `response_entity`.
	switch(request_headers.verb){
	case Poseidon::Http::V_HEAD:
	case Poseidon::Http::V_GET:
	case Poseidon::Http::V_POST: {
		const AUTO(foyer_conn, FoyerConnector::get_connection());
		DEBUG_THROW_UNLESS(foyer_conn, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_GATEWAY);

		Protocol::FG_HttpResponse foyer_resp;
		{
			Protocol::GF_HttpRequest foyer_req;
			foyer_req.client_uuid = get_session_uuid();
			foyer_req.client_ip   = get_remote_info().ip();
			foyer_req.auth_token  = m_auth_token.get();
			foyer_req.verb        = request_headers.verb;
			foyer_req.decoded_uri = m_decoded_uri;
			Protocol::copy_key_values(foyer_req.params, STD_MOVE(request_headers.get_params));
			Protocol::copy_key_values(foyer_req.headers, STD_MOVE(request_headers.headers));
			foyer_req.entity      = request_entity.dump_byte_string();
			LOG_CIRCE_TRACE("Sending request: ", foyer_req);
			Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
			LOG_CIRCE_TRACE("Received response: ", foyer_resp);
		}
		response_headers.status_code = foyer_resp.status_code;
		response_headers.headers     = Protocol::copy_key_values(STD_MOVE(foyer_resp.headers));
		response_entity_as_string    = STD_MOVE(foyer_resp.entity);
		break; }

	case Poseidon::Http::V_OPTIONS: {
		response_headers.status_code = Poseidon::Http::ST_OK;
		break; }

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}

	// Fill in other fields.
	const AUTO(desc, Poseidon::Http::get_status_code_desc(response_headers.status_code));
	response_headers.reason = desc.desc_short;
	const AUTO(keep_alive, Poseidon::Http::is_keep_alive_enabled(request_headers) && (response_headers.status_code / 100 <= 3));
	response_headers.headers.set(Poseidon::sslit("Connection"), keep_alive ? "Keep-Alive" : "Close");
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Origin"), "*");
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Headers"), "Authorization, Content-Type");

	// Select an encoding for compression if the response entity is not empty and no explicit encoding is given.
	Poseidon::Http::ContentEncoding response_encoding;
	const std::size_t size_original = response_entity_as_string.size();
	if(size_original == 0){
		response_encoding = Poseidon::Http::CE_IDENTITY;
	} else if(response_headers.headers.has("Content-Encoding")){
		response_encoding = Poseidon::Http::CE_IDENTITY;
	} else if(response_encoding_preferred == Poseidon::Http::CE_GZIP){
		response_encoding = Poseidon::Http::CE_GZIP;
	} else if(response_encoding_preferred == Poseidon::Http::CE_DEFLATE){
		response_encoding = Poseidon::Http::CE_DEFLATE;
	} else {
		response_encoding = Poseidon::Http::CE_IDENTITY;
	}
	// Compress it if possible
	Poseidon::StreamBuffer response_entity;
	switch(response_encoding){
	case Poseidon::Http::CE_GZIP: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using GZIP: compression_level = ", compression_level);
		Poseidon::Deflator deflator(true, compression_level);
		deflator.put(response_entity_as_string.data(), response_entity_as_string.size());
		response_entity = deflator.finalize();
		const std::size_t size_deflated = response_entity.size();
		LOG_CIRCE_TRACE("GZIP result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
		response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "gzip");
		break; }
	case Poseidon::Http::CE_DEFLATE: {
		const AUTO(compression_level, get_config<int>("client_http_compression_level", 8));
		LOG_CIRCE_TRACE("Compressing HTTP response using DEFLATE: compression_level = ", compression_level);
		Poseidon::Deflator deflator(false, compression_level);
		deflator.put(response_entity_as_string.data(), response_entity_as_string.size());
		response_entity = deflator.finalize();
		const std::size_t size_deflated = response_entity.size();
		LOG_CIRCE_TRACE("DEFLATE result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
		response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "deflate");
		break; }
	default: {
		response_entity.put(response_entity_as_string.data(), response_entity_as_string.size());
		break; }
	}
	// For 4xx and 5xx responses, create a default entity if there isn't one.
	if((response_headers.status_code / 100 >= 4) && response_entity.empty()){
		AUTO(pair, Poseidon::Http::make_default_response(response_headers.status_code, STD_MOVE(response_headers.headers)));
		response_headers = STD_MOVE(pair.first);
		response_entity  = STD_MOVE(pair.second);
	}
	send(STD_MOVE(response_headers), STD_MOVE(response_entity));

	// HTTP is stateless.
	m_decoded_uri.clear();
	m_auth_token.reset();
}

bool ClientHttpSession::has_been_shutdown() const NOEXCEPT {
	PROFILE_ME;

	return Poseidon::Http::Session::has_been_shutdown_write();
}
bool ClientHttpSession::shutdown() NOEXCEPT {
	PROFILE_ME;

	const AUTO(ws_session, boost::dynamic_pointer_cast<ClientWebSocketSession>(get_upgraded_session()));
	if(ws_session){
		return ws_session->shutdown(Poseidon::WebSocket::ST_GOING_AWAY, "");
	}
	LOG_CIRCE_DEBUG("Shutting down HTTP client: remote = ", get_remote_info());
	shutdown_read();
	return shutdown_write();
}

bool ClientHttpSession::send(Poseidon::Http::ResponseHeaders response_headers, Poseidon::StreamBuffer entity){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending to HTTP client: remote = ", get_remote_info(), ", status_code = ", response_headers.status_code, ", headers = ", response_headers.headers, ", entity.size() = ", entity.size());
	return Poseidon::Http::Session::send(STD_MOVE(response_headers), STD_MOVE(entity));
}
bool ClientHttpSession::send_default_and_shutdown(Poseidon::Http::StatusCode status_code, const Poseidon::OptionalMap &headers) NOEXCEPT {
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Shutting down HTTP client: remote = ", get_remote_info(), ", status_code = ", status_code, ", headers = ", headers);
	return Poseidon::Http::Session::send_default_and_shutdown(status_code, headers);
}

}
}
