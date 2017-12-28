// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_shadow_session.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_foyer.hpp"
#include "singletons/box_acceptor.hpp"
#include <poseidon/job_base.hpp>

namespace Circe {
namespace Box {

class ClientWebSocketShadowSession::OutgoingShutdownJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<ClientWebSocketShadowSession> m_weak_session;

	Poseidon::Uuid m_foyer_uuid;
	Poseidon::Uuid m_gate_uuid;
	Poseidon::Uuid m_client_uuid;
	Poseidon::WebSocket::StatusCode m_status_code;
	std::string m_reason;

public:
	OutgoingShutdownJob(const boost::shared_ptr<ClientWebSocketShadowSession> &session, const Poseidon::Uuid &foyer_uuid, const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid,
		Poseidon::WebSocket::StatusCode status_code, std::string reason)
		: m_weak_session(session)
		, m_foyer_uuid(foyer_uuid), m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_status_code(status_code), m_reason(STD_MOVE(reason))
	{ }

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_session;
	}
	void perform(){
		PROFILE_ME;

		const AUTO(foyer_conn, BoxAcceptor::get_session(m_foyer_uuid));
		if(!foyer_conn){
			return;
		}

		try {
			Protocol::Foyer::WebSocketKillRequestToGate foyer_ntfy;
			foyer_ntfy.gate_uuid   = m_gate_uuid;
			foyer_ntfy.client_uuid = m_client_uuid;
			foyer_ntfy.status_code = m_status_code;
			foyer_ntfy.reason      = STD_MOVE(m_reason);
			LOG_CIRCE_TRACE("Sending notification: ", foyer_ntfy);
			foyer_conn->send_notification(foyer_ntfy);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
		}
	}
};

class ClientWebSocketShadowSession::OutgoingDeliveryJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<ClientWebSocketShadowSession> m_weak_session;
	std::string m_reason;

public:
	explicit OutgoingDeliveryJob(const boost::shared_ptr<ClientWebSocketShadowSession> &session)
		: m_weak_session(session)
	{ }

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_session;
	}
	void perform(){
		PROFILE_ME;

		const AUTO(session, m_weak_session.lock());
		if(!session){
			return;
		}

		try {
			const AUTO(foyer_conn, BoxAcceptor::get_session(session->m_foyer_uuid));
			DEBUG_THROW_UNLESS(foyer_conn, Poseidon::Exception, Poseidon::sslit("Connection to foyer server was lost"));

			Poseidon::Mutex::UniqueLock lock(session->m_delivery_mutex);
			DEBUG_THROW_ASSERT(session->m_delivery_job_active);
			for(;;){
				LOG_CIRCE_DEBUG("Collecting messages pending: session = ", (void *)session.get(), ", count = ", session->m_messages_pending.size());
				if(session->m_messages_pending.empty()){
					break;
				}
				// Send messages that have been enqueued.
				Protocol::Foyer::WebSocketPackedMessageRequestToGate foyer_req;
				foyer_req.gate_uuid   = session->get_gate_uuid();
				foyer_req.client_uuid = session->get_client_uuid();
				while(!session->m_messages_pending.empty()){
					foyer_req.messages.emplace_back();
					foyer_req.messages.back().opcode  = session->m_messages_pending.front().first;
					foyer_req.messages.back().payload = STD_MOVE(session->m_messages_pending.front().second);
					session->m_messages_pending.pop_front();
				}
				lock.unlock();
				LOG_CIRCE_TRACE("Sending request: ", foyer_req);
				Protocol::Foyer::WebSocketPackedMessageResponseFromGate foyer_resp;
				Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
				LOG_CIRCE_TRACE("Received response: ", foyer_resp);
				DEBUG_THROW_UNLESS(foyer_resp.delivered, Poseidon::Exception, Poseidon::sslit("Message delivery failed"));
				lock.lock();
			}
			session->m_delivery_job_active = false;
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}
};

ClientWebSocketShadowSession::ClientWebSocketShadowSession(const Poseidon::Uuid &foyer_uuid, const Poseidon::Uuid &gate_uuid, const Poseidon::Uuid &client_uuid, std::string client_ip, std::string auth_token)
	: m_foyer_uuid(foyer_uuid), m_gate_uuid(gate_uuid), m_client_uuid(client_uuid), m_client_ip(STD_MOVE(client_ip)), m_auth_token(STD_MOVE(auth_token))
	, m_has_been_shutdown(false)
{
	LOG_CIRCE_INFO("ClientWebSocketShadowSession constructor: client_ip = ", get_client_ip());
}
ClientWebSocketShadowSession::~ClientWebSocketShadowSession(){
	LOG_CIRCE_INFO("ClientWebSocketShadowSession destructor: client_ip = ", get_client_ip());
}

void ClientWebSocketShadowSession::on_sync_open(){
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: ClientWebSocketShadowSession::on_sync_open()");
}
void ClientWebSocketShadowSession::on_sync_receive(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: ClientWebSocketShadowSession::on_sync_receive(): opcode = ", opcode, ", payload = ", payload);
}
void ClientWebSocketShadowSession::on_sync_close(Poseidon::WebSocket::StatusCode status_code, const char *reason){
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: ClientWebSocketShadowSession::on_sync_close(): status_code = ", status_code, ", reason = ", reason);
}

bool ClientWebSocketShadowSession::has_been_shutdown() const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_delivery_mutex);
	return m_has_been_shutdown;
}
bool ClientWebSocketShadowSession::shutdown(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_delivery_mutex);
	if(m_has_been_shutdown){
		return false;
	}
	const AUTO(foyer_conn, BoxAcceptor::get_session(m_foyer_uuid));
	if(foyer_conn){
		try {
			Poseidon::enqueue(boost::make_shared<OutgoingShutdownJob>(virtual_shared_from_this<ClientWebSocketShadowSession>(), m_foyer_uuid, m_gate_uuid, m_client_uuid, status_code, reason));
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
		}
	}
	m_has_been_shutdown = true;
	return true;
}
bool ClientWebSocketShadowSession::send(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_delivery_mutex);
	if(m_has_been_shutdown){
		return false;
	}
	if(!m_delivery_job_spare){
		m_delivery_job_spare = boost::make_shared<OutgoingDeliveryJob>(virtual_shared_from_this<ClientWebSocketShadowSession>());
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
