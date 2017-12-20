// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
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

	LOG_POSEIDON_FATAL("TODO CHECK AUTHENTICATION");
	return "websocket";
}

void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("CONTROL: ", opcode, ": ", payload);
}
void ClientWebSocketSession::on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("CONTROL: ", opcode, ": ", payload);
}

}
}
