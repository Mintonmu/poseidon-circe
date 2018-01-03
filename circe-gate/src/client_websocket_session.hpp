// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_
#define CIRCE_GATE_CLIENT_WEBSOCKET_SESSION_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/websocket/session.hpp>
#include <poseidon/uuid.hpp>
#include <boost/optional.hpp>
#include <boost/container/deque.hpp>
#include <utility>
#include "common/fwd.hpp"

namespace Circe {
namespace Gate {

class ClientHttpSession;

class ClientWebSocketSession : public Poseidon::WebSocket::Session {
	friend ClientHttpSession;

private:
	class DeliveryJob;

private:
	const Poseidon::Uuid m_client_uuid;

	// These are accessed only by the epoll thread.
	boost::uint64_t m_request_counter_reset_time;
	boost::uint64_t m_request_counter;

	// These are accessed only by the primary thread.
	boost::weak_ptr<Common::InterserverConnection> m_weak_foyer_conn;
	boost::optional<Poseidon::Uuid> m_box_uuid;
	boost::optional<std::pair<Poseidon::WebSocket::StatusCode, std::string> > m_closure_reason;

	boost::optional<std::string> m_auth_token;

	boost::shared_ptr<DeliveryJob> m_delivery_job_spare;
	bool m_delivery_job_active;
	boost::container::deque<std::pair<Poseidon::WebSocket::OpCode, Poseidon::StreamBuffer> > m_messages_pending;

public:
	explicit ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent);
	~ClientWebSocketSession() OVERRIDE;

private:
	// This function should be only called by the destructor.
	void notify_foyer_about_closure() const NOEXCEPT;

	void sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params);

protected:
	// Callbacks run in the epoll thread.
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
