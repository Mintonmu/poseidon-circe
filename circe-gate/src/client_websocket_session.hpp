// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_
#define CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_

#include <poseidon/fwd.hpp>
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
	class WebSocketDeliveryJob;
	class WebSocketClosureJob;

private:
	static void on_closure_notification_low_level_timer(const boost::shared_ptr<ClientWebSocketSession> &session);

private:
	const Poseidon::Uuid m_client_uuid;

	mutable Poseidon::Mutex m_closure_notification_mutex;
	boost::shared_ptr<Poseidon::Timer> m_closure_notification_timer;
	boost::optional<std::pair<Poseidon::WebSocket::StatusCode, std::string> > m_closure_reason;

	// These are accessed only by the epoll thread.
	boost::uint64_t m_request_counter_reset_time;
	boost::uint64_t m_request_counter;

	// These are accessed only by the primary thread.
	boost::optional<std::string> m_auth_token;

	boost::container::deque<std::pair<Poseidon::WebSocket::StatusCode, Poseidon::StreamBuffer> > m_messages_pending;
	boost::shared_ptr<WebSocketDeliveryJob> m_delivery_job;
	bool m_delivery_job_active;

public:
	explicit ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent);
	~ClientWebSocketSession() OVERRIDE;

private:
	void reserve_closure_notification_timer();
	void drop_closure_notification_timer() NOEXCEPT;
	void deliver_closure_notification(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT;

	std::string sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params);

protected:
	// Callbacks run in the epoll thread.
	void on_close(int err_code) OVERRIDE;
	bool on_low_level_message_end(boost::uint64_t whole_size) OVERRIDE;

	// Callbacks run in the primary thread.
	void on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload) OVERRIDE;
	void on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload) OVERRIDE;

public:
	const Poseidon::Uuid &get_client_uuid() const NOEXCEPT {
		return m_client_uuid;
	}

	bool has_been_shutdown() const NOEXCEPT;
	bool shutdown(Poseidon::WebSocket::StatusCode status_code, const char *reason = "") NOEXCEPT;

	bool send(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload);
};

}
}

#endif
