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

class Client_http_session;

class Client_websocket_session : public Poseidon::Websocket::Session {
	friend Client_http_session;

private:
	class Delivery_job;

private:
	const Poseidon::Uuid m_client_uuid;

	// These are accessed only by the primary thread.
	boost::weak_ptr<Common::Interserver_connection> m_weak_foyer_conn;
	boost::optional<Poseidon::Uuid> m_box_uuid;
	boost::optional<std::pair<Poseidon::Websocket::Status_code, std::string> > m_closure_reason;

	boost::optional<std::string> m_auth_token;

	mutable Poseidon::Mutex m_delivery_mutex;
	boost::shared_ptr<Delivery_job> m_delivery_job_spare;
	bool m_delivery_job_active;
	boost::container::deque<std::pair<Poseidon::Websocket::Op_code, Poseidon::Stream_buffer> > m_messages_pending;

public:
	explicit Client_websocket_session(const boost::shared_ptr<Client_http_session> &parent);
	~Client_websocket_session() OVERRIDE;

private:
	// This function should be only called by the destructor.
	void notify_foyer_about_closure() const NOEXCEPT;

	void sync_authenticate(const std::string &decoded_uri, const Poseidon::Option_map &params);

protected:
	// Callbacks run in the epoll thread.
	bool on_low_level_message_end(boost::uint64_t whole_size) OVERRIDE;

	// Callbacks run in the primary thread.
	void on_sync_data_message(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload) OVERRIDE;
	void on_sync_control_message(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload) OVERRIDE;

public:
	const Poseidon::Uuid &get_client_uuid() const NOEXCEPT {
		return m_client_uuid;
	}

	bool has_been_shutdown() const NOEXCEPT;
	bool shutdown(Poseidon::Websocket::Status_code status_code, const char *reason = "") NOEXCEPT;

	bool send(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload);
};

}
}

#endif
