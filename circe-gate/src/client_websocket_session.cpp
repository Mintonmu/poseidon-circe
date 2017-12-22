// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
#include "singletons/auth_connector.hpp"
#include "common/cbpp_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/websocket/exception.hpp>

namespace Circe {
namespace Gate {

ClientWebSocketSession::ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent)
	: Poseidon::WebSocket::Session(parent)
	, m_session_uuid(parent->get_session_uuid())
{
	LOG_CIRCE_DEBUG("ClientWebSocketSession constructor: remote = ", get_remote_info());
}
ClientWebSocketSession::~ClientWebSocketSession(){
	LOG_CIRCE_DEBUG("ClientWebSocketSession destructor: remote = ", get_remote_info());
}

std::string ClientWebSocketSession::sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params) const {
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
	return STD_MOVE(auth_resp.auth_token);
}

void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("DATA: ", opcode, ": ", payload);
}

}
}
