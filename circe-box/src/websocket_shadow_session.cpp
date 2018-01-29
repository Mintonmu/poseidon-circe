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

class WebSocketShadowSession::ShutdownJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<WebSocketShadowSession> m_weak_session;
	const boost::weak_ptr<Common::InterserverConnection> m_weak_foyer_conn;

	Poseidon::Uuid m_gate_uuid;
	Poseidon::Uuid m_client_uuid;
	Poseidon::WebSocket::StatusCode m_status_code;
	std::string m_reason;

public:
	ShutdownJob(const boost::shared_ptr<WebSocketShadowSession> &session, const boost::shared_ptr<Common::InterserverConnection> &foyer_conn,
		const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, Poseidon::WebSocket::StatusCode status_code, std::string reason)
		: m_weak_session(session), m_weak_foyer_conn(foyer_conn)
		, m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_status_code(status_code), m_reason(STD_MOVE(reason))
	{ }

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
			Protocol::Foyer::WebSocketKillNotificationToGate foyer_ntfy;
			foyer_ntfy.gate_uuid   = m_gate_uuid;
			foyer_ntfy.client_uuid = m_client_uuid;
			foyer_ntfy.status_code = m_status_code;
			foyer_ntfy.reason      = STD_MOVE(m_reason);
			foyer_conn->send_notification(foyer_ntfy);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
		}
	}
};

class WebSocketShadowSession::DeliveryJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<WebSocketShadowSession> m_weak_session;

public:
	explicit DeliveryJob(const boost::shared_ptr<WebSocketShadowSession> &session)
		: m_weak_session(session)
	{ }

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
			const AUTO(foyer_conn, BoxAcceptor::get_session(session->m_foyer_uuid));
			DEBUG_THROW_UNLESS(foyer_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("Connection to foyer server was lost"));

			Poseidon::Mutex::UniqueLock lock(session->m_delivery_mutex);
			DEBUG_THROW_ASSERT(session->m_delivery_job_active);
			for(;;){
				VALUE_TYPE(session->m_messages_pending) messages_pending;
				messages_pending.swap(session->m_messages_pending);
				LOG_CIRCE_TRACE("Messages pending: session = ", (void *)session.get(), ", count = ", messages_pending.size());
				if(messages_pending.empty()){
					break;
				}
				lock.unlock();

				Protocol::Foyer::WebSocketPackedMessageRequestToGate foyer_req;
				foyer_req.gate_uuid   = session->m_gate_uuid;
				foyer_req.client_uuid = session->m_client_uuid;
				for(AUTO(qmit, messages_pending.begin()); qmit != messages_pending.end(); ++qmit){
					const AUTO(rmit, Protocol::emplace_at_end(foyer_req.messages));
					rmit->opcode  = boost::numeric_cast<boost::uint8_t>(qmit->first);
					rmit->payload = STD_MOVE(qmit->second);
				}
				LOG_CIRCE_TRACE("Sending request: ", foyer_req);
				Protocol::Foyer::WebSocketPackedMessageResponseFromGate foyer_resp;
				Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
				LOG_CIRCE_TRACE("Received response: ", foyer_resp);

				lock.lock();
			}
			session->m_delivery_job_active = false;
		} catch(Poseidon::WebSocket::Exception &e){
			LOG_CIRCE_ERROR("Poseidon::WebSocket::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			session->shutdown(e.get_status_code(), e.what());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}
};

WebSocketShadowSession::WebSocketShadowSession(const Poseidon::Uuid &foyer_uuid, const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, std::string client_ip, std::string auth_token)
	: m_foyer_uuid(foyer_uuid), m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_client_ip(STD_MOVE(client_ip)), m_auth_token(STD_MOVE(auth_token))
	, m_shutdown(false)
{
	LOG_CIRCE_INFO("WebSocketShadowSession constructor: client_uuid = ", m_client_uuid, ", client_ip = ", m_client_ip);
}
WebSocketShadowSession::~WebSocketShadowSession(){
	LOG_CIRCE_INFO("WebSocketShadowSession destructor: client_uuid = ", m_client_uuid, ", client_ip = ", m_client_ip);
}

bool WebSocketShadowSession::has_been_shutdown() const {
	return Poseidon::atomic_load(m_shutdown, Poseidon::memorder_acquire);
}
void WebSocketShadowSession::mark_shutdown() NOEXCEPT {
	Poseidon::atomic_store(m_shutdown, true, Poseidon::memorder_release);
}
bool WebSocketShadowSession::shutdown(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	bool was_shutdown = Poseidon::atomic_load(m_shutdown, Poseidon::memorder_acquire);
	if(!was_shutdown){
		was_shutdown = Poseidon::atomic_exchange(m_shutdown, true, Poseidon::memorder_acq_rel);
	}
	if(was_shutdown){
		return false;
	}
	const AUTO(foyer_conn, BoxAcceptor::get_session(m_foyer_uuid));
	if(foyer_conn){
		try {
			Poseidon::enqueue(boost::make_shared<ShutdownJob>(virtual_shared_from_this<WebSocketShadowSession>(), foyer_conn, m_gate_uuid, m_client_uuid, status_code, reason));
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
		}
	}
	return true;
}
bool WebSocketShadowSession::send(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;
	LOG_CIRCE_TRACE("Sending message via WebSocketShadowSession: client_ip = ", get_client_ip(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	if(has_been_shutdown()){
		LOG_CIRCE_DEBUG("WebSocketShadowSessio has been shut down: client_ip = ", get_client_ip());
		return false;
	}

	const Poseidon::Mutex::UniqueLock lock(m_delivery_mutex);
	if(!m_delivery_job_spare){
		m_delivery_job_spare = boost::make_shared<DeliveryJob>(virtual_shared_from_this<WebSocketShadowSession>());
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
