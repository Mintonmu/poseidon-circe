// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
#include "mmain.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/interserver_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_auth.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/utilities.hpp"
#include "singletons/ip_ban_list.hpp"
#include <poseidon/job_base.hpp>
#include <poseidon/sock_addr.hpp>
#include <poseidon/websocket/exception.hpp>

namespace Circe {
namespace Gate {

class ClientWebSocketSession::DeliveryJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<Poseidon::TcpSessionBase> m_weak_parent;
	const boost::weak_ptr<ClientWebSocketSession> m_weak_ws_session;

public:
	explicit DeliveryJob(const boost::shared_ptr<ClientWebSocketSession> &ws_session)
		: m_weak_parent(ws_session->get_weak_parent()), m_weak_ws_session(ws_session)
	{ }

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_parent;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(ws_session, m_weak_ws_session.lock());
		if(!ws_session){
			return;
		}

		try {
			const AUTO(foyer_conn, ws_session->m_weak_foyer_conn.lock());
			DEBUG_THROW_UNLESS(foyer_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("Connection to foyer server was lost"));

			DEBUG_THROW_ASSERT(ws_session->m_delivery_job_active);
			for(;;){
				VALUE_TYPE(ws_session->m_messages_pending) messages_pending;
				messages_pending.swap(ws_session->m_messages_pending);
				LOG_CIRCE_TRACE("Messages pending: ws_session = ", (void *)ws_session.get(), ", count = ", messages_pending.size());
				if(messages_pending.empty()){
					break;
				}

				Protocol::Foyer::WebSocketPackedMessageRequestToBox foyer_req;
				foyer_req.box_uuid    = ws_session->m_box_uuid.get();
				foyer_req.client_uuid = ws_session->m_client_uuid;
				for(AUTO(qmit, messages_pending.begin()); qmit != messages_pending.end(); ++qmit){
					const AUTO(rmit, Protocol::emplace_at_end(foyer_req.messages));
					rmit->opcode  = boost::numeric_cast<boost::uint8_t>(qmit->first);
					rmit->payload = STD_MOVE(qmit->second);
				}
				LOG_CIRCE_TRACE("Sending request: ", foyer_req);
				Protocol::Foyer::WebSocketPackedMessageResponseFromBox foyer_resp;
				Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
				LOG_CIRCE_TRACE("Received response: ", foyer_resp);
			}
			ws_session->m_delivery_job_active = false;
		} catch(Poseidon::WebSocket::Exception &e){
			LOG_CIRCE_ERROR("Poseidon::WebSocket::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			ws_session->shutdown(e.get_status_code(), e.what());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}
};

ClientWebSocketSession::ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent)
	: Poseidon::WebSocket::Session(parent)
	, m_client_uuid(parent->m_client_uuid)
{
	LOG_CIRCE_DEBUG("ClientWebSocketSession constructor: remote = ", get_remote_info());
}
ClientWebSocketSession::~ClientWebSocketSession(){
	notify_foyer_about_closure();
	LOG_CIRCE_DEBUG("ClientWebSocketSession destructor: remote = ", get_remote_info());
}

void ClientWebSocketSession::notify_foyer_about_closure() const NOEXCEPT {
	PROFILE_ME;

	const AUTO(foyer_conn, m_weak_foyer_conn.lock());
	if(!foyer_conn){
		return;
	}

	try {
		Protocol::Foyer::WebSocketClosureNotificationToBox foyer_ntfy;
		foyer_ntfy.box_uuid    = m_box_uuid.get();
		foyer_ntfy.client_uuid = m_client_uuid;
		const AUTO(reason_ptr, m_closure_reason.get_ptr());
		foyer_ntfy.status_code = reason_ptr ? reason_ptr->first : static_cast<unsigned>(Poseidon::WebSocket::ST_RESERVED_ABNORMAL);
		foyer_ntfy.reason      = reason_ptr ? reason_ptr->second : "No closure frame received";
		foyer_conn->send_notification(foyer_ntfy);
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		foyer_conn->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
	}
}

void ClientWebSocketSession::sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params)
try {
	PROFILE_ME;

	const AUTO(websocket_enabled, get_config<bool>("client_websocket_enabled", false));
	DEBUG_THROW_UNLESS(websocket_enabled, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("WebSocket is not enabled"));

	boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > servers_avail;
	AuthConnector::get_all_clients(servers_avail);
	DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("No auth server is available"));
	const AUTO(auth_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(auth_conn);

	Protocol::Auth::WebSocketAuthenticationRequest auth_req;
	auth_req.client_uuid = m_client_uuid;
	auth_req.client_ip   = get_remote_info().ip();
	auth_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(auth_req.params, params);
	LOG_CIRCE_TRACE("Sending request: ", auth_req);
	Protocol::Auth::WebSocketAuthenticationResponse auth_resp;
	Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
	LOG_CIRCE_TRACE("Received response: ", auth_resp);
	for(AUTO(qmit, auth_resp.messages.begin()); qmit != auth_resp.messages.end(); ++qmit){
		send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(qmit->opcode), STD_MOVE(qmit->payload));
	}
	DEBUG_THROW_UNLESS(!auth_resp.auth_token.empty(), Poseidon::WebSocket::Exception, boost::numeric_cast<Poseidon::WebSocket::StatusCode>(auth_resp.status_code), Poseidon::SharedNts(auth_resp.reason));
	LOG_CIRCE_DEBUG("Auth server has allowed WebSocket client: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);

	servers_avail.clear();
	FoyerConnector::get_all_clients(servers_avail);
	DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("No foyer server is available"));
	const AUTO(foyer_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(foyer_conn);

	Protocol::Foyer::WebSocketEstablishmentRequestToBox foyer_req;
	foyer_req.client_uuid = m_client_uuid;
	foyer_req.client_ip   = get_remote_info().ip();
	foyer_req.auth_token  = auth_resp.auth_token;
	foyer_req.decoded_uri = decoded_uri;
	Protocol::copy_key_values(foyer_req.params, params);
	LOG_CIRCE_TRACE("Sending request: ", foyer_req);
	Protocol::Foyer::WebSocketEstablishmentResponseFromBox foyer_resp;
	Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
	LOG_CIRCE_TRACE("Received response: ", foyer_resp);
	m_weak_foyer_conn = foyer_conn;
	m_box_uuid        = Poseidon::Uuid(foyer_resp.box_uuid);

	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::sslit("Connection has been shut down"));
	LOG_CIRCE_DEBUG("Established WebSocketConnection: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	m_auth_token = STD_MOVE_IDN(auth_resp.auth_token);
} catch(...){
	IpBanList::accumulate_auth_failure(get_remote_info().ip());
	throw;
}

bool ClientWebSocketSession::on_low_level_message_end(boost::uint64_t whole_size){
	PROFILE_ME;

	const AUTO(exempt_private, get_config<bool>("client_generic_exempt_private_addresses", true));
	if(exempt_private && Poseidon::SockAddr(get_remote_info()).is_private()){
		LOG_CIRCE_DEBUG("Client exempted: ", get_remote_info());
	} else {
		IpBanList::accumulate_websocket_request(get_remote_info().ip());
	}

	return Poseidon::WebSocket::Session::on_low_level_message_end(whole_size);
}

void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Received WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	if(!m_delivery_job_spare){
		m_delivery_job_spare = boost::make_shared<DeliveryJob>(virtual_shared_from_this<ClientWebSocketSession>());
		m_delivery_job_active = false;
	}
	if(!m_delivery_job_active){
		Poseidon::enqueue(m_delivery_job_spare);
		m_delivery_job_active = true;
	}
	// Enqueue the message if the new fiber has been created successfully.
	const AUTO(max_requests_pending, get_config<boost::uint64_t>("client_websocket_max_requests_pending", 100));
	DEBUG_THROW_UNLESS(m_messages_pending.size() < max_requests_pending, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("Max number of requests pending exceeded"));
	LOG_CIRCE_TRACE("Enqueueing message: opcode = ", opcode, ", payload.size() = ", payload.size());
	m_messages_pending.emplace_back(opcode, STD_MOVE(payload));
}
void ClientWebSocketSession::on_sync_control_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;
	LOG_CIRCE_TRACE("Received WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	if(opcode == Poseidon::WebSocket::OP_CLOSE){
		char data[125];
		std::size_t size = payload.peek(data, sizeof(data));
		if(size == 0){
			m_closure_reason = std::make_pair(Poseidon::WebSocket::ST_NORMAL_CLOSURE, "");
		} else if(size < 2){
			m_closure_reason = std::make_pair(Poseidon::WebSocket::ST_INACCEPTABLE, "No enough bytes in WebSocket closure frame");
		} else {
			boost::uint16_t temp16;
			std::memcpy(&temp16, data, 2);
			m_closure_reason = std::make_pair(Poseidon::load_be(temp16), std::string(data + 2, size - 2));
		}
	}

	if(opcode == Poseidon::WebSocket::OP_PONG){
		LOG_CIRCE_DEBUG("Received PONG from client: remote = ", get_remote_info(), ", payload = ", payload);
	}

	Poseidon::WebSocket::Session::on_sync_control_message(opcode, STD_MOVE(payload));
}

bool ClientWebSocketSession::has_been_shutdown() const NOEXCEPT {
	PROFILE_ME;

	return Poseidon::WebSocket::Session::has_been_shutdown_write();
}
bool ClientWebSocketSession::shutdown(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Shutting down WebSocket connection: remote = ", get_remote_info(), ", status_code = ", status_code, ", reason = ", reason);

	return Poseidon::WebSocket::Session::shutdown(status_code, reason);
}

bool ClientWebSocketSession::send(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;
	LOG_CIRCE_TRACE("Sending WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	return Poseidon::WebSocket::Session::send(opcode, STD_MOVE(payload), false);
}

}
}
