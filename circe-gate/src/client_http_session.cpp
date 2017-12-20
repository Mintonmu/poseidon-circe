// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_session.hpp"
#include "client_websocket_session.hpp"
#include "singletons/client_http_acceptor.hpp"
#include "mmain.hpp"
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
	const TcpSessionBase::DelayedShutdownGuard m_guard;
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
	void perform() final {
		PROFILE_ME;

		const AUTO(http_session, m_weak_http_session.lock());
		if(!http_session || http_session->has_been_shutdown_write()){
			return;
		}
		const AUTO(ws_session, m_weak_ws_session.lock());
		if(!ws_session){
			http_session->force_shutdown();
			return;
		}
		LOG_CIRCE_TRACE("Client WebSocket establishment: uri = ", m_request_headers.uri, ", headers = ", m_request_headers.headers);

		std::string decoded_uri;
		try {
			const AUTO(enable_websocket, get_config<bool>("protocol_enable_websocket", false));
			DEBUG_THROW_UNLESS(enable_websocket, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

			Poseidon::Buffer_istream bis;
			bis.set_buffer(Poseidon::StreamBuffer(m_request_headers.uri));
			Poseidon::Http::url_decode(bis, decoded_uri);
			DEBUG_THROW_UNLESS(bis, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_REQUEST);
			LOG_CIRCE_TRACE("Decoded URI: ", decoded_uri);

			AUTO(ws_resp, Poseidon::WebSocket::make_handshake_response(m_request_headers));
			DEBUG_THROW_UNLESS(ws_resp.status_code == Poseidon::Http::ST_SWITCHING_PROTOCOLS, Poseidon::Http::Exception, ws_resp.status_code, STD_MOVE(ws_resp.headers));
			http_session->send(STD_MOVE(ws_resp));
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
			LOG_POSEIDON_FATAL("TODO WEBSOCKET ESTABLISHED");
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

ClientHttpSession::ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket)
	: Poseidon::Http::Session(STD_MOVE(socket))
{
	LOG_CIRCE_DEBUG("ClientHttpSession constructor: remote = ", get_remote_info());
}
ClientHttpSession::~ClientHttpSession(){
	LOG_CIRCE_DEBUG("ClientHttpSession destructor: remote = ", get_remote_info());
	ClientHttpAcceptor::remove_session(this);
}

void ClientHttpSession::sync_authenticate(const Poseidon::Http::RequestHeaders &request_headers){
	PROFILE_ME;

	std::string decoded_uri;
	Poseidon::Buffer_istream bis;
	bis.set_buffer(Poseidon::StreamBuffer(request_headers.uri));
	Poseidon::Http::url_decode(bis, decoded_uri);
	DEBUG_THROW_UNLESS(bis, Poseidon::Http::Exception, Poseidon::Http::ST_BAD_REQUEST);
	LOG_CIRCE_TRACE("Decoded URI: ", decoded_uri);

	std::string auth_token;
	LOG_POSEIDON_FATAL("TODO CHECK AUTHENTICATION");
	auth_token = "meow";

	m_decoded_uri = STD_MOVE(decoded_uri);
	m_auth_token = STD_MOVE(auth_token);
}

boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> ClientHttpSession::on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers){
	PROFILE_ME;

	const AUTO_REF(request_headers, Poseidon::Http::Session::get_low_level_request_headers());
	LOG_CIRCE_TRACE("Client HTTP request: verb = ", Poseidon::Http::get_string_from_verb(request_headers.verb), ", uri = ", request_headers.uri, ", headers = ", request_headers.headers);

	const AUTO_REF(upgrade_str, request_headers.headers.get("Upgrade"));
	if(::strcasecmp(upgrade_str.c_str(), "websocket") == 0){
		AUTO(ws_session, boost::make_shared<ClientWebSocketSession>(virtual_shared_from_this<ClientHttpSession>()));
		Poseidon::enqueue(boost::make_shared<WebSocketHandshakeJob>(virtual_shared_from_this<ClientHttpSession>(), request_headers, ws_session));
		return STD_MOVE(ws_session);
	} else if(upgrade_str.empty()){
		return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
	} else {
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}
}

void ClientHttpSession::on_sync_expect(Poseidon::Http::RequestHeaders request_headers){
	PROFILE_ME;

	// Check authentication early.
	sync_authenticate(request_headers);
	LOG_CIRCE_DEBUG("Authenticated: remote = ", get_remote_info(), ", decoded_uri = ", m_decoded_uri, ", auth_token = ", m_auth_token);
	DEBUG_THROW_ASSERT(!m_decoded_uri.empty());

	return Poseidon::Http::Session::on_sync_expect(STD_MOVE(request_headers));
}
void ClientHttpSession::on_sync_request(Poseidon::Http::RequestHeaders request_headers, Poseidon::StreamBuffer request_entity){
	PROFILE_ME;

	// If no `Expect:` header was received, check authentication now.
	if(m_decoded_uri.empty()){
		sync_authenticate(request_headers);
		LOG_CIRCE_DEBUG("Authenticated: remote = ", get_remote_info(), ", decoded_uri = ", m_decoded_uri, ", auth_token = ", m_auth_token);
	}
	DEBUG_THROW_ASSERT(!m_decoded_uri.empty());

	const AUTO(response_encoding, Poseidon::Http::pick_content_encoding(request_headers));
	DEBUG_THROW_UNLESS(response_encoding != Poseidon::Http::CE_NOT_ACCEPTABLE, Poseidon::Http::Exception, Poseidon::Http::ST_NOT_ACCEPTABLE);

	// Read then clear these members.
	const AUTO(decoded_uri, STD_MOVE(m_decoded_uri));
	m_decoded_uri.clear();
	const AUTO(auth_token, STD_MOVE(m_auth_token));
	m_auth_token.clear();

	Poseidon::Http::ResponseHeaders response_headers = { 10001 };
	Poseidon::StreamBuffer response_entity;

	// Process the request. `HEAD` is handled in the same way as `GET`.
	// Fill in `response_headers.status_code` and `response_entity`.
	switch(request_headers.verb){
	case Poseidon::Http::V_HEAD:
	case Poseidon::Http::V_GET: {
		const AUTO(enable_http, get_config<bool>("protocol_enable_http", false));
		DEBUG_THROW_UNLESS(enable_http, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

		response_headers.status_code = Poseidon::Http::ST_OK;
		for(int i = 0; i < 1000; ++i){
			response_entity.put("meow ");
		}
		break; }

	case Poseidon::Http::V_POST: {
		const AUTO(enable_http, get_config<bool>("protocol_enable_http", false));
		DEBUG_THROW_UNLESS(enable_http, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

		response_headers.status_code = Poseidon::Http::ST_OK;
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
	if(Poseidon::Http::is_keep_alive_enabled(request_headers) && (response_headers.status_code / 100 <= 3)){
		response_headers.headers.set(Poseidon::sslit("Connection"), "Keep-Alive");
	} else {
		response_headers.headers.set(Poseidon::sslit("Connection"), "Close");
	}
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Origin"), "*");
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Headers"), "Authorization, Content-Type");

	// Try compressing the response entity if it is not empty and no explicit encoding is given.
	const std::size_t size_original = response_entity.size();
	if((size_original != 0) && !response_headers.headers.has("Content-Encoding")){
		const AUTO(compression_level, get_config<int>("protocol_http_compression_level", 6));
		if(compression_level > 0){
			switch(response_encoding){
			case Poseidon::Http::CE_GZIP: {
				LOG_CIRCE_TRACE("Compressing HTTP response using GZIP: compression_level = ", compression_level);
				response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "gzip");
				Poseidon::Deflator deflator(true, compression_level);
				deflator.put(response_entity);
				response_entity = deflator.finalize();
				const std::size_t size_deflated = response_entity.size();
				LOG_CIRCE_TRACE("GZIP result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
				break; }
			case Poseidon::Http::CE_DEFLATE: {
				LOG_CIRCE_TRACE("Compressing HTTP response using DEFLATE: compression_level = ", compression_level);
				response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "deflate");
				Poseidon::Deflator deflator(false, compression_level);
				deflator.put(response_entity);
				response_entity = deflator.finalize();
				const std::size_t size_deflated = response_entity.size();
				LOG_CIRCE_TRACE("DEFLATE result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
				break; }
			default: {
				LOG_CIRCE_TRACE("The client did not send any `Accept-Encoding` we support.");
				break; }
			}
		}
	}
	if((response_headers.status_code / 100 >= 3) && response_entity.empty()){
		send_default(response_headers.status_code, STD_MOVE(response_headers.headers));
	} else {
		send(STD_MOVE(response_headers), STD_MOVE(response_entity));
	}
}

}
}
