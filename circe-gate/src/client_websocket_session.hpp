// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_
#define CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_

#include <poseidon/websocket/session.hpp>
#include <poseidon/uuid.hpp>
#include <boost/optional.hpp>
#include "common/fwd.hpp"

namespace Circe {
namespace Gate {

class ClientHttpSession;

class ClientWebSocketSession : public Poseidon::WebSocket::Session {
	friend ClientHttpSession;

private:
	class WebSocketClosureJob;

private:
	const Poseidon::Uuid m_session_uuid;

	mutable Poseidon::Mutex m_foyer_conn_mutex;
	boost::optional<boost::weak_ptr<Common::InterserverConnection> > m_weak_foyer_conn;

	boost::optional<std::string> m_auth_token;

public:
	explicit ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent);
	~ClientWebSocketSession() OVERRIDE;

private:
	bool is_foyer_conn_set_but_expired() const NOEXCEPT;
	boost::shared_ptr<Common::InterserverConnection> get_foyer_conn() const NOEXCEPT;
	boost::shared_ptr<Common::InterserverConnection> release_foyer_conn() NOEXCEPT;
	void link_foyer_conn(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn);

	std::string sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params);
	void sync_notify_foyer_about_closure(Poseidon::WebSocket::StatusCode status_code, std::string message) NOEXCEPT;

protected:
	void on_close(int err_code) OVERRIDE;
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
