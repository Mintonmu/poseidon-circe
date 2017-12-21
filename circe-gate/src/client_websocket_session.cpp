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

	const AUTO(auth_connection, AuthConnector::get_connection());
	DEBUG_THROW_UNLESS(auth_connection, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY);

	Protocol::GA_ClientWebSocketAuthenticationRequest req;
	req.session_uuid = get_session_uuid();
	req.client_ip    = get_remote_info().ip();
	req.decoded_uri  = decoded_uri;
	Protocol::copy_key_values(req.params, params);
	LOG_CIRCE_TRACE("Sending request: ", req);
	AUTO(result, Poseidon::wait(auth_connection->send_request(req)));
	DEBUG_THROW_UNLESS(result.get_err_code() == Protocol::ERR_SUCCESS, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_INTERNAL_ERROR);

	Protocol::AG_ClientWebSocketAuthenticationResponse resp;
	DEBUG_THROW_UNLESS(result.get_message_id() == resp.get_id(), Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_INTERNAL_ERROR);
	resp.deserialize(result.get_payload());
	LOG_CIRCE_TRACE("Received response: ", req);
	if(resp.websocket_status_code != 0){
		DEBUG_THROW(Poseidon::WebSocket::Exception, resp.websocket_status_code, Poseidon::SharedNts(resp.message));
	}
	return STD_MOVE(resp.auth_token);
}

void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("DATA: ", opcode, ": ", payload);
}
void ClientWebSocketSession::on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("CONTROL: ", opcode, ": ", payload);
}

}
}
