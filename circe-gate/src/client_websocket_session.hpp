// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_
#define CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_

#include <poseidon/websocket/session.hpp>
#include <poseidon/uuid.hpp>

namespace Circe {
namespace Gate {

class ClientHttpSession;

class ClientWebSocketSession : public Poseidon::WebSocket::Session {
private:
	const Poseidon::Uuid m_session_uuid;

public:
	explicit ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent);
	~ClientWebSocketSession() OVERRIDE;

protected:
	void on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload) OVERRIDE;
	void on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload) OVERRIDE;

public:
	const Poseidon::Uuid &get_session_uuid() const NOEXCEPT {
		return m_session_uuid;
	}
};

}
}

#endif
