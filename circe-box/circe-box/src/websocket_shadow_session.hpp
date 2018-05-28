// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_BOX_WEBSOCKET_SHADOW_SESSION_HPP_
#define CIRCE_BOX_WEBSOCKET_SHADOW_SESSION_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/uuid.hpp>
#include <poseidon/mutex.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include <poseidon/websocket/opcodes.hpp>
#include <poseidon/stream_buffer.hpp>
#include <boost/container/deque.hpp>
#include <string>

namespace Circe {
namespace Box {

class Websocket_shadow_session : public virtual Poseidon::Virtual_shared_from_this {
private:
	class Shutdown_job;
	class Delivery_job;

private:
	const Poseidon::Uuid m_foyer_uuid;
	const Poseidon::Uuid m_gate_uuid;
	const Poseidon::Uuid m_client_uuid;
	const std::string m_client_ip;
	const std::string m_auth_token;

	volatile bool m_shutdown;

	mutable Poseidon::Mutex m_delivery_mutex;
	boost::shared_ptr<Delivery_job> m_delivery_job_spare;
	bool m_delivery_job_active;
	boost::container::deque<std::pair<Poseidon::Websocket::Opcode, Poseidon::Stream_buffer> > m_messages_pending;

public:
	Websocket_shadow_session(const Poseidon::Uuid &foyer_uuid, const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, std::string client_ip, std::string auth_token);
	~Websocket_shadow_session() OVERRIDE;

public:
	const Poseidon::Uuid &get_foyer_uuid() const {
		return m_foyer_uuid;
	}
	const Poseidon::Uuid &get_gate_uuid() const {
		return m_gate_uuid;
	}
	const Poseidon::Uuid &get_client_uuid() const {
		return m_client_uuid;
	}
	const std::string &get_client_ip() const {
		return m_client_ip;
	}
	const std::string &get_auth_token() const {
		return m_auth_token;
	}

	bool has_been_shutdown() const;
	void mark_shutdown() NOEXCEPT;
	bool shutdown(Poseidon::Websocket::Status_code status_code, const char *reason = "") NOEXCEPT;
	bool send(Poseidon::Websocket::Opcode opcode, Poseidon::Stream_buffer payload);
};

}
}

#endif
