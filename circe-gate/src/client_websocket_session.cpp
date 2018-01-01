// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
#include "mmain.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/cbpp_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_auth.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/singletons/timer_daemon.hpp>
#include <poseidon/websocket/exception.hpp>
#include <poseidon/job_base.hpp>

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
			const AUTO(foyer_conn, FoyerConnector::get_client(ws_session->m_foyer_uuid.get()));
			DEBUG_THROW_UNLESS(foyer_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("Connection to foyer server was lost"));

			DEBUG_THROW_ASSERT(ws_session->m_delivery_job_active);
			for(;;){
				LOG_CIRCE_DEBUG("Collecting messages pending: ws_session = ", (void *)ws_session.get(), ", count = ", ws_session->m_messages_pending.size());
				if(ws_session->m_messages_pending.empty()){
					break;
				}
				// Send messages that have been enqueued.
				Protocol::Foyer::WebSocketPackedMessageRequestToBox foyer_req;
				foyer_req.box_uuid    = ws_session->m_box_uuid.get();
				foyer_req.client_uuid = ws_session->m_client_uuid;
				while(!ws_session->m_messages_pending.empty()){
					foyer_req.messages.emplace_back();
					foyer_req.messages.back().opcode  = ws_session->m_messages_pending.front().first;
					foyer_req.messages.back().payload = STD_MOVE(ws_session->m_messages_pending.front().second);
					ws_session->m_messages_pending.pop_front();
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

class ClientWebSocketSession::ClosureJob : public Poseidon::JobBase {
private:
	const boost::weak_ptr<Poseidon::TcpSessionBase> m_weak_parent;
	const boost::weak_ptr<Common::InterserverConnection> m_weak_foyer_conn;

	Poseidon::Uuid m_box_uuid;
	Poseidon::Uuid m_client_uuid;
	Poseidon::WebSocket::StatusCode m_status_code;
	std::string m_reason;

public:
	ClosureJob(const boost::shared_ptr<ClientWebSocketSession> &ws_session, const boost::shared_ptr<Common::InterserverConnection> &foyer_conn,
		const Poseidon::Uuid &box_uuid, const Poseidon::Uuid &client_uuid, Poseidon::WebSocket::StatusCode status_code, std::string reason)
		: m_weak_parent(ws_session->get_weak_parent()), m_weak_foyer_conn(foyer_conn)
		, m_box_uuid(box_uuid), m_client_uuid(client_uuid), m_status_code(status_code), m_reason(STD_MOVE(reason))
	{ }

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_parent;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(foyer_conn, m_weak_foyer_conn.lock());
		if(!foyer_conn){
			return;
		}

		try {
			Protocol::Foyer::WebSocketClosureNotificationToBox foyer_ntfy;
			foyer_ntfy.box_uuid    = m_box_uuid;
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

ClientWebSocketSession::ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent)
	: Poseidon::WebSocket::Session(parent)
	, m_client_uuid(parent->m_client_uuid)
	, m_request_counter_reset_time(0), m_request_counter(0)
{
	LOG_CIRCE_DEBUG("ClientWebSocketSession constructor: remote = ", get_remote_info());
}
ClientWebSocketSession::~ClientWebSocketSession(){
	LOG_CIRCE_DEBUG("ClientWebSocketSession destructor: remote = ", get_remote_info());
}

void ClientWebSocketSession::on_closure_notification_low_level_timer(){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_closure_notification_mutex);
	if(!m_closure_reason){
		return;
	}
	LOG_CIRCE_DEBUG("Reaping WebSocket client: this = ", (void *)this);
	if(m_foyer_uuid){
		const AUTO(foyer_conn, FoyerConnector::get_client(m_foyer_uuid.get()));
		if(foyer_conn){
			// Enqueue a job to notify the foyer server about closure of this connection.
			// If this operation throws an exception, the timer will be rerun, so there is no cleanup to be done.
			Poseidon::enqueue(boost::make_shared<ClosureJob>(virtual_shared_from_this<ClientWebSocketSession>(), foyer_conn,
				m_box_uuid.get(), m_client_uuid, m_closure_reason.get().first, m_closure_reason.get().second));
		}
	}
	// If the job has been enqueued successfully, we can break the circular reference, destroying this timer.
	m_closure_notification_timer.reset();
}

void ClientWebSocketSession::reserve_closure_notification_timer(){
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Reserving WebSocket closure notification timer: this = ", (void *)this);

	const Poseidon::Mutex::UniqueLock lock(m_closure_notification_mutex);
	DEBUG_THROW_ASSERT(!m_closure_notification_timer);
	// Create a circular reference. We do this deliberately to prevent the session from being deleted after it is detached from epoll.
	const AUTO(timer, Poseidon::TimerDaemon::register_low_level_timer(60000, 60000,
		boost::bind(&ClientWebSocketSession::on_closure_notification_low_level_timer, virtual_shared_from_this<ClientWebSocketSession>())));
	m_closure_notification_timer = timer;
}
void ClientWebSocketSession::drop_closure_notification_timer() NOEXCEPT {
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Dropping WebSocket closure notification timer: this = ", (void *)this);

	const Poseidon::Mutex::UniqueLock lock(m_closure_notification_mutex);
	m_closure_notification_timer.reset();
}
void ClientWebSocketSession::deliver_closure_notification(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Delivering closure notification: this = ", (void *)this);

	const Poseidon::Mutex::UniqueLock lock(m_closure_notification_mutex);
	if(!m_closure_reason){
		m_closure_reason = std::make_pair(status_code, reason);
	}
	if(m_closure_notification_timer){
		Poseidon::TimerDaemon::set_time(m_closure_notification_timer, 0);
	}
}

void ClientWebSocketSession::sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params){
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
	while(!auth_resp.messages.empty()){
		send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(auth_resp.messages.front().opcode), STD_MOVE(auth_resp.messages.front().payload));
		auth_resp.messages.pop_front();
	}
	DEBUG_THROW_UNLESS(!auth_resp.auth_token.empty(), Poseidon::WebSocket::Exception, boost::numeric_cast<Poseidon::WebSocket::StatusCode>(auth_resp.status_code), Poseidon::SharedNts(auth_resp.reason));
	LOG_CIRCE_DEBUG("Auth server has allowed WebSocket client: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);

	servers_avail.clear();
	FoyerConnector::get_all_clients(servers_avail);
	DEBUG_THROW_UNLESS(servers_avail.size() != 0, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("No foyer server is available"));
	const AUTO(foyer_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(foyer_conn);

	Protocol::Foyer::WebSocketEstablishmentResponseFromBox foyer_resp;
	reserve_closure_notification_timer();
	try {
		Protocol::Foyer::WebSocketEstablishmentRequestToBox foyer_req;
		foyer_req.client_uuid = m_client_uuid;
		foyer_req.client_ip   = get_remote_info().ip();
		foyer_req.auth_token  = auth_resp.auth_token;
		foyer_req.decoded_uri = decoded_uri;
		Protocol::copy_key_values(foyer_req.params, params);
		LOG_CIRCE_TRACE("Sending request: ", foyer_req);
		Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
		LOG_CIRCE_TRACE("Received response: ", foyer_resp);
	} catch(...){
		drop_closure_notification_timer();
		throw;
	}

	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::sslit("Connection has been shut down"));
	LOG_CIRCE_DEBUG("Established WebSocketConnection: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	m_auth_token = STD_MOVE_IDN(auth_resp.auth_token);
	m_box_uuid   = Poseidon::Uuid(foyer_resp.box_uuid);
	m_foyer_uuid = foyer_conn->get_connection_uuid();
}

void ClientWebSocketSession::on_close(int err_code){
	PROFILE_ME;
	LOG_CIRCE_DEBUG("WebSocket connection closed: remote = ", get_remote_info(), ", err_code = ", err_code);

	Poseidon::Buffer_ostream bos;
	bos <<"WebSocket connection closed without a closure frame: Socket error was " <<err_code <<" (" <<Poseidon::get_error_desc(err_code) <<")" <<std::ends;
	deliver_closure_notification(Poseidon::WebSocket::ST_RESERVED_ABNORMAL, static_cast<const char *>(bos.get_buffer().squash()));

	Poseidon::WebSocket::Session::on_close(err_code);
}
bool ClientWebSocketSession::on_low_level_message_end(boost::uint64_t whole_size){
	PROFILE_ME;

	const AUTO(now, Poseidon::get_fast_mono_clock());
	if(m_request_counter_reset_time < now){
		m_request_counter = 0;
		m_request_counter_reset_time = Poseidon::saturated_add<boost::uint64_t>(now, 60000);
	}
	const AUTO(max_requests_per_minute, get_config<boost::uint64_t>("client_websocket_max_requests_per_minute", 300));
	DEBUG_THROW_UNLESS(m_request_counter < max_requests_per_minute, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY, Poseidon::sslit("Max number of requests per minute exceeded"));
	++m_request_counter;

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
		Poseidon::WebSocket::StatusCode status_code;
		const char *reason;
		// Truncate the reason string if it is too long.
		char data[125 + 1];
		// Make room for the null terminator.
		std::size_t size = payload.peek(data, sizeof(data) - 1);
		if(size == 0){
			status_code = Poseidon::WebSocket::ST_NORMAL_CLOSURE;
			reason = "";
		} else if(size < 2){
			status_code = Poseidon::WebSocket::ST_INACCEPTABLE;
			reason = "No enough bytes in WebSocket closure frame";
		} else {
			boost::uint16_t temp16;
			std::memcpy(&temp16, data, 2);
			status_code = Poseidon::load_be(temp16);
			data[size] = 0;
			reason = data + 2;
		}
		deliver_closure_notification(status_code, reason);
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

	if(Poseidon::WebSocket::Session::has_been_shutdown_write()){
		return false;
	}
	deliver_closure_notification(status_code, reason);
	return Poseidon::WebSocket::Session::shutdown(status_code, reason);
}

bool ClientWebSocketSession::send(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;
	LOG_CIRCE_DEBUG("Sending WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	return Poseidon::WebSocket::Session::send(opcode, STD_MOVE(payload), false);
}

}
}
