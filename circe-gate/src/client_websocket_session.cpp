// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/cbpp_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"
#include "protocol/messages_gate_foyer.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/websocket/exception.hpp>
#include <poseidon/job_base.hpp>

namespace Circe {
namespace Gate {

class ClientWebSocketSession::WebSocketClosureJob : public Poseidon::JobBase {
private:
	boost::shared_ptr<ClientWebSocketSession> m_ws_session;
	Poseidon::WebSocket::StatusCode m_status_code;
	std::string m_message;

public:
	WebSocketClosureJob(const boost::shared_ptr<ClientWebSocketSession> &ws_session, Poseidon::WebSocket::StatusCode status_code, std::string message)
		: m_ws_session(ws_session), m_status_code(status_code), m_message(STD_MOVE(message))
	{ }

public:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_ws_session;
	}
	void perform() FINAL {
		m_ws_session->sync_notify_foyer_about_closure(m_status_code, STD_MOVE(m_message));
	}
};

ClientWebSocketSession::ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent)
	: Poseidon::WebSocket::Session(parent)
	, m_session_uuid(parent->get_session_uuid())
{
	LOG_CIRCE_DEBUG("ClientWebSocketSession constructor: remote = ", get_remote_info());
}
ClientWebSocketSession::~ClientWebSocketSession(){
	LOG_CIRCE_DEBUG("ClientWebSocketSession destructor: remote = ", get_remote_info());
}

bool ClientWebSocketSession::is_foyer_conn_set_but_expired() const NOEXCEPT {
	const Poseidon::Mutex::UniqueLock lock(m_foyer_conn_mutex);
	const AUTO(ptr, m_weak_foyer_conn.get_ptr());
	if(!ptr){
		return false;
	}
	return ptr->expired();
}
boost::shared_ptr<Common::InterserverConnection> ClientWebSocketSession::get_foyer_conn() const NOEXCEPT {
	const Poseidon::Mutex::UniqueLock lock(m_foyer_conn_mutex);
	const AUTO(ptr, m_weak_foyer_conn.get_ptr());
	if(!ptr){
		return VAL_INIT;
	}
	return ptr->lock();
}
boost::shared_ptr<Common::InterserverConnection> ClientWebSocketSession::release_foyer_conn() NOEXCEPT {
	const Poseidon::Mutex::UniqueLock lock(m_foyer_conn_mutex);
	const AUTO(ptr, m_weak_foyer_conn.get_ptr());
	if(!ptr){
		return VAL_INIT;
	}
	AUTO(foyer_conn, ptr->lock());
	ptr->reset();
	return STD_MOVE_IDN(foyer_conn);
}
void ClientWebSocketSession::link_foyer_conn(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn){
	const Poseidon::Mutex::UniqueLock lock(m_foyer_conn_mutex);
	DEBUG_THROW_ASSERT(!m_weak_foyer_conn);
	m_weak_foyer_conn = foyer_conn;
}

std::string ClientWebSocketSession::sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params){
	PROFILE_ME;

	const AUTO(auth_conn, AuthConnector::get_connection());
	DEBUG_THROW_UNLESS(auth_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY);
	Protocol::GA_ClientWebSocketAuthenticationRequest auth_req;
	auth_req.client_uuid = get_session_uuid();
	auth_req.client_ip   = get_remote_info().ip();
	auth_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(auth_req.params, params);
	LOG_CIRCE_TRACE("Sending request: ", auth_req);
	Protocol::AG_ClientWebSocketAuthenticationResponse auth_resp;
	Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
	LOG_CIRCE_TRACE("Received response: ", auth_resp);
	if(auth_resp.status_code != 0){
		DEBUG_THROW(Poseidon::WebSocket::Exception, auth_resp.status_code, Poseidon::SharedNts(auth_resp.message));
	}

	const AUTO(foyer_conn, FoyerConnector::get_connection());
	DEBUG_THROW_UNLESS(foyer_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY);
	Protocol::GF_ClientWebSocketEstablishment foyer_req;
	foyer_req.client_uuid = get_session_uuid();
	foyer_req.client_ip   = get_remote_info().ip();
	foyer_req.auth_token  = auth_resp.auth_token;
	foyer_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(auth_req.params, params);
	LOG_CIRCE_TRACE("Sending request: ", foyer_req);
	Protocol::FG_ClientWebSocketAcceptance foyer_resp;
	Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
	LOG_CIRCE_TRACE("Received response: ", foyer_resp);
	if(foyer_resp.status_code != 0){
		DEBUG_THROW(Poseidon::WebSocket::Exception, foyer_resp.status_code, Poseidon::SharedNts(foyer_resp.message));
	}
	link_foyer_conn(foyer_conn);

	return STD_MOVE(auth_resp.auth_token);
}
void ClientWebSocketSession::sync_notify_foyer_about_closure(Poseidon::WebSocket::StatusCode status_code, std::string message) NOEXCEPT {
	PROFILE_ME;

	const AUTO(foyer_conn, release_foyer_conn());
	if(!foyer_conn){
		return;
	}
	try {
		Protocol::GF_ClientWebSocketClosure foyer_ntfy;
		foyer_ntfy.client_uuid = get_session_uuid();
		foyer_ntfy.client_ip   = get_remote_info().ip();
		foyer_ntfy.status_code = status_code;
		foyer_ntfy.message     = STD_MOVE(message);
		foyer_conn->send_notification(foyer_ntfy);
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
	}
}

void ClientWebSocketSession::on_close(int err_code){
	PROFILE_ME;

	const AUTO(foyer_conn, get_foyer_conn());
	if(foyer_conn){
		try {
			Poseidon::enqueue(boost::make_shared<WebSocketClosureJob>(virtual_shared_from_this<ClientWebSocketSession>(), Poseidon::WebSocket::ST_RESERVED_ABNORMAL, std::string()));
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
		}
	}

	Poseidon::WebSocket::Session::on_close(err_code);
}
void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("DATA: ", opcode, ": ", payload);
}
void ClientWebSocketSession::on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	if(opcode == Poseidon::WebSocket::OP_CLOSE){
		sync_notify_foyer_about_closure(Poseidon::WebSocket::ST_NORMAL_CLOSURE, payload.dump_string());
	}

	Poseidon::WebSocket::Session::on_sync_control_message(opcode, STD_MOVE(payload));
}

}
}
