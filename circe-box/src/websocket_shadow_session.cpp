// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "websocket_shadow_session.hpp"
#include "singletons/box_acceptor.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/websocket/exception.hpp>
#include <poseidon/job_base.hpp>

namespace Circe {
namespace Box {

class Web_socket_shadow_session::Shutdown_job : public Poseidon::Job_base {
private:
	const boost::weak_ptr<Web_socket_shadow_session> m_weak_session;
	const boost::weak_ptr<Common::Interserver_connection> m_weak_foyer_conn;

	Poseidon::Uuid m_gate_uuid;
	Poseidon::Uuid m_client_uuid;
	Poseidon::Web_socket::Status_code m_status_code;
	std::string m_reason;

public:
	Shutdown_job(const boost::shared_ptr<Web_socket_shadow_session> &session, const boost::shared_ptr<Common::Interserver_connection> &foyer_conn,
		const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, Poseidon::Web_socket::Status_code status_code, std::string reason)
		: m_weak_session(session), m_weak_foyer_conn(foyer_conn)
		, m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_status_code(status_code), m_reason(STD_MOVE(reason))
	{
		//
	}

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_session;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(foyer_conn, m_weak_foyer_conn.lock());
		if(!foyer_conn){
			return;
		}

		try {
			Protocol::Foyer::Web_socket_kill_notification_to_gate foyer_ntfy;
			foyer_ntfy.gate_uuid   = m_gate_uuid;
			foyer_ntfy.client_uuid = m_client_uuid;
			foyer_ntfy.status_code = m_status_code;
			foyer_ntfy.reason      = STD_MOVE(m_reason);
			foyer_conn->send_notification(foyer_ntfy);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::error_internal_error, e.what());
		}
	}
};

class Web_socket_shadow_session::Delivery_job : public Poseidon::Job_base {
private:
	const boost::weak_ptr<Web_socket_shadow_session> m_weak_session;

public:
	explicit Delivery_job(const boost::shared_ptr<Web_socket_shadow_session> &session)
		: m_weak_session(session)
	{
		//
	}

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_session;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(session, m_weak_session.lock());
		if(!session){
			return;
		}

		try {
			const AUTO(foyer_conn, Box_acceptor::get_session(session->m_foyer_uuid));
			DEBUG_THROW_UNLESS(foyer_conn, Poseidon::Web_socket::Exception, Poseidon::Web_socket::status_going_away, Poseidon::sslit("Connection to foyer server was lost"));

			Poseidon::Mutex::Unique_lock lock(session->m_delivery_mutex);
			DEBUG_THROW_ASSERT(session->m_delivery_job_active);
			for(;;){
				VALUE_TYPE(session->m_messages_pending) messages_pending;
				messages_pending.swap(session->m_messages_pending);
				LOG_CIRCE_TRACE("Messages pending: session = ", (void *)session.get(), ", count = ", messages_pending.size());
				if(messages_pending.empty()){
					break;
				}
				lock.unlock();

				Protocol::Foyer::Web_socket_packed_message_request_to_gate foyer_req;
				foyer_req.gate_uuid   = session->m_gate_uuid;
				foyer_req.client_uuid = session->m_client_uuid;
				for(AUTO(qmit, messages_pending.begin()); qmit != messages_pending.end(); ++qmit){
					const AUTO(rmit, Protocol::emplace_at_end(foyer_req.messages));
					rmit->opcode  = boost::numeric_cast<boost::uint8_t>(qmit->first);
					rmit->payload = STD_MOVE(qmit->second);
				}
				LOG_CIRCE_TRACE("Sending request: ", foyer_req);
				Protocol::Foyer::Web_socket_packed_message_response_from_gate foyer_resp;
				Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
				LOG_CIRCE_TRACE("Received response: ", foyer_resp);

				lock.lock();
			}
			session->m_delivery_job_active = false;
		} catch(Poseidon::Web_socket::Exception &e){
			LOG_CIRCE_ERROR("Poseidon::Web_socket::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			session->shutdown(e.get_status_code(), e.what());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			session->shutdown(Poseidon::Web_socket::status_internal_error, e.what());
		}
	}
};

Web_socket_shadow_session::Web_socket_shadow_session(const Poseidon::Uuid &foyer_uuid, const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, std::string client_ip, std::string auth_token)
	: m_foyer_uuid(foyer_uuid), m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_client_ip(STD_MOVE(client_ip)), m_auth_token(STD_MOVE(auth_token))
	, m_shutdown(false)
{
	LOG_CIRCE_INFO("Web_socket_shadow_session constructor: client_uuid = ", m_client_uuid, ", client_ip = ", m_client_ip);
}
Web_socket_shadow_session::~Web_socket_shadow_session(){
	LOG_CIRCE_INFO("Web_socket_shadow_session destructor: client_uuid = ", m_client_uuid, ", client_ip = ", m_client_ip);
}

bool Web_socket_shadow_session::has_been_shutdown() const {
	return Poseidon::atomic_load(m_shutdown, Poseidon::memory_order_acquire);
}
void Web_socket_shadow_session::mark_shutdown() NOEXCEPT {
	Poseidon::atomic_store(m_shutdown, true, Poseidon::memory_order_release);
}
bool Web_socket_shadow_session::shutdown(Poseidon::Web_socket::Status_code status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	bool was_shutdown = Poseidon::atomic_load(m_shutdown, Poseidon::memory_order_acquire);
	if(!was_shutdown){
		was_shutdown = Poseidon::atomic_exchange(m_shutdown, true, Poseidon::memory_order_acq_rel);
	}
	if(was_shutdown){
		return false;
	}
	const AUTO(foyer_conn, Box_acceptor::get_session(m_foyer_uuid));
	if(foyer_conn){
		try {
			Poseidon::enqueue(boost::make_shared<Shutdown_job>(virtual_shared_from_this<Web_socket_shadow_session>(), foyer_conn, m_gate_uuid, m_client_uuid, status_code, reason));
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::error_internal_error, e.what());
		}
	}
	return true;
}
bool Web_socket_shadow_session::send(Poseidon::Web_socket::Op_code opcode, Poseidon::Stream_buffer payload){
	PROFILE_ME;
	LOG_CIRCE_TRACE("Sending message via Web_socket_shadow_session: client_ip = ", get_client_ip(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	if(has_been_shutdown()){
		LOG_CIRCE_DEBUG("Web_socket_shadow_sessio has been shut down: client_ip = ", get_client_ip());
		return false;
	}

	const Poseidon::Mutex::Unique_lock lock(m_delivery_mutex);
	if(!m_delivery_job_spare){
		m_delivery_job_spare = boost::make_shared<Delivery_job>(virtual_shared_from_this<Web_socket_shadow_session>());
		m_delivery_job_active = false;
	}
	if(!m_delivery_job_active){
		Poseidon::enqueue(m_delivery_job_spare);
		m_delivery_job_active = true;
	}
	m_messages_pending.emplace_back(opcode, STD_MOVE(payload));
	return true;
}

}
}
