// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_
#define CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_

#include <poseidon/websocket/session.hpp>
#include <poseidon/uuid.hpp>
#include <boost/optional.hpp>

namespace Circe {
namespace Gate {

class ClientHttpSession;

class ClientWebSocketSession : public Poseidon::WebSocket::Session {
	friend ClientHttpSession;

private:
	const Poseidon::Uuid m_session_uuid;

	boost::optional<std::string> m_auth_token;

public:
	explicit ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent);
	~ClientWebSocketSession() OVERRIDE;

private:
	std::string sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params);
	void sync_notify_closure(Poseidon::WebSocket::StatusCode status_code, std::string message);

protected:
	void on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload) OVERRIDE;

public:
	const Poseidon::Uuid &get_session_uuid() const NOEXCEPT {
		return m_session_uuid;
	}
};

}
}

#endif
