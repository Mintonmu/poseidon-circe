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
#include "singletons/ip_ban_list.hpp"
#include <poseidon/job_base.hpp>
#include <poseidon/sock_addr.hpp>
#include <poseidon/websocket/exception.hpp>

namespace Circe {
namespace Gate {

class Client_websocket_session::Delivery_job : public Poseidon::Job_base {
private:
	const boost::weak_ptr<Poseidon::Tcp_session_base> m_weak_parent;
	const boost::weak_ptr<Client_websocket_session> m_weak_ws_session;

public:
	explicit Delivery_job(const boost::shared_ptr<Client_websocket_session> &ws_session)
		: m_weak_parent(ws_session->get_weak_parent()), m_weak_ws_session(ws_session)
	{
		//
	}

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		return m_weak_parent;
	}
	void perform() FINAL {
		POSEIDON_PROFILE_ME;

		const AUTO(ws_session, m_weak_ws_session.lock());
		if(!ws_session){
			return;
		}

		try {
			const AUTO(foyer_conn, ws_session->m_weak_foyer_conn.lock());
			POSEIDON_THROW_UNLESS(foyer_conn, Poseidon::Websocket::Exception, Poseidon::Websocket::status_going_away, Poseidon::Rcnts::view("Connection to foyer server was lost"));

			Poseidon::Mutex::Unique_lock lock(ws_session->m_delivery_mutex);
			POSEIDON_THROW_ASSERT(ws_session->m_delivery_job_active);
			for(;;){
				VALUE_TYPE(ws_session->m_messages_pending) messages_pending;
				messages_pending.swap(ws_session->m_messages_pending);
				CIRCE_LOG_TRACE("Messages pending: ws_session = ", (void *)ws_session.get(), ", count = ", messages_pending.size());
				if(messages_pending.empty()){
					break;
				}
				lock.unlock();

				Protocol::Foyer::Websocket_packed_message_request_to_box foyer_req;
				foyer_req.box_uuid    = ws_session->m_box_uuid.get();
				foyer_req.client_uuid = ws_session->m_client_uuid;
				for(AUTO(it, messages_pending.begin()); it != messages_pending.end(); ++it){
					Protocol::Common_websocket_frame frame;
					frame.opcode  = boost::numeric_cast<boost::uint8_t>(it->first);
					frame.payload = STD_MOVE(it->second);
					foyer_req.messages.push_back(STD_MOVE(frame));
				}
				CIRCE_LOG_TRACE("Sending request: ", foyer_req);
				Protocol::Foyer::Websocket_packed_message_response_from_box foyer_resp;
				Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
				CIRCE_LOG_TRACE("Received response: ", foyer_resp);

				lock.lock();
			}
			ws_session->m_delivery_job_active = false;
		} catch(Poseidon::Websocket::Exception &e){
			CIRCE_LOG_ERROR("Poseidon::Websocket::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
			ws_session->shutdown(e.get_status_code(), e.what());
		} catch(std::exception &e){
			CIRCE_LOG_ERROR("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::Websocket::status_internal_error, e.what());
		}
	}
};

Client_websocket_session::Client_websocket_session(const boost::shared_ptr<Client_http_session> &parent)
	: Poseidon::Websocket::Session(parent)
	, m_client_uuid(parent->m_client_uuid)
{
	CIRCE_LOG_DEBUG("Client_websocket_session constructor: remote = ", get_remote_info());
}
Client_websocket_session::~Client_websocket_session(){
	notify_foyer_about_closure();
	CIRCE_LOG_DEBUG("Client_websocket_session destructor: remote = ", get_remote_info());
}

void Client_websocket_session::notify_foyer_about_closure() const NOEXCEPT {
	POSEIDON_PROFILE_ME;

	const AUTO(foyer_conn, m_weak_foyer_conn.lock());
	if(!foyer_conn){
		return;
	}

	try {
		Protocol::Foyer::Websocket_closure_notification_to_box foyer_ntfy;
		foyer_ntfy.box_uuid    = m_box_uuid.get();
		foyer_ntfy.client_uuid = m_client_uuid;
		const AUTO(reason_ptr, m_closure_reason.get_ptr());
		foyer_ntfy.status_code = reason_ptr ? reason_ptr->first : static_cast<unsigned>(Poseidon::Websocket::status_reserved_abnormal);
		foyer_ntfy.reason      = reason_ptr ? reason_ptr->second : "No closure frame received";
		foyer_conn->send_notification(foyer_ntfy);
	} catch(std::exception &e){
		CIRCE_LOG_ERROR("std::exception thrown: what = ", e.what());
		foyer_conn->shutdown(Protocol::error_internal_error, e.what());
	}
}

void Client_websocket_session::sync_authenticate(const std::string &decoded_uri, const Poseidon::Option_map &params)
try {
	POSEIDON_PROFILE_ME;

	const AUTO(websocket_enabled, get_config<bool>("client_websocket_enabled", false));
	POSEIDON_THROW_UNLESS(websocket_enabled, Poseidon::Websocket::Exception, Poseidon::Websocket::status_going_away, Poseidon::Rcnts::view("WebSocket is not enabled"));

	boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > servers_avail;
	Auth_connector::get_all_clients(servers_avail);
	POSEIDON_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Websocket::Exception, Poseidon::Websocket::status_going_away, Poseidon::Rcnts::view("No auth server is available"));
	const AUTO(auth_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	POSEIDON_THROW_ASSERT(auth_conn);

	Protocol::Auth::Websocket_authentication_request auth_req;
	auth_req.client_uuid = m_client_uuid;
	auth_req.client_ip   = get_remote_info().ip();
	auth_req.decoded_uri = decoded_uri;
	for(AUTO(it, params.begin()); it != params.end(); ++it){
		Protocol::Common_key_value option;
		option.key   = it->first.get();
		option.value = STD_MOVE(it->second);
		auth_req.params.push_back(STD_MOVE(option));
	}
	CIRCE_LOG_TRACE("Sending request: ", auth_req);
	Protocol::Auth::Websocket_authentication_response auth_resp;
	Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
	CIRCE_LOG_TRACE("Received response: ", auth_resp);
	for(AUTO(it, auth_resp.messages.begin()); it != auth_resp.messages.end(); ++it){
		send(boost::numeric_cast<Poseidon::Websocket::Op_code>(it->opcode), STD_MOVE(it->payload));
	}
	POSEIDON_THROW_UNLESS(!auth_resp.auth_token.empty(), Poseidon::Websocket::Exception, boost::numeric_cast<Poseidon::Websocket::Status_code>(auth_resp.status_code), Poseidon::Rcnts(auth_resp.reason));
	CIRCE_LOG_DEBUG("Auth server has allowed WebSocket client: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);

	servers_avail.clear();
	Foyer_connector::get_all_clients(servers_avail);
	POSEIDON_THROW_UNLESS(servers_avail.size() != 0, Poseidon::Websocket::Exception, Poseidon::Websocket::status_going_away, Poseidon::Rcnts::view("No foyer server is available"));
	const AUTO(foyer_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	POSEIDON_THROW_ASSERT(foyer_conn);

	Protocol::Foyer::Websocket_establishment_request_to_box foyer_req;
	foyer_req.client_uuid = m_client_uuid;
	foyer_req.client_ip   = get_remote_info().ip();
	foyer_req.auth_token  = auth_resp.auth_token;
	foyer_req.decoded_uri = decoded_uri;
	for(AUTO(it, params.begin()); it != params.end(); ++it){
		Protocol::Common_key_value option;
		option.key   = it->first.get();
		option.value = STD_MOVE(it->second);
		foyer_req.params.push_back(STD_MOVE(option));
	}
	CIRCE_LOG_TRACE("Sending request: ", foyer_req);
	Protocol::Foyer::Websocket_establishment_response_from_box foyer_resp;
	Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
	CIRCE_LOG_TRACE("Received response: ", foyer_resp);
	m_weak_foyer_conn = foyer_conn;
	m_box_uuid        = Poseidon::Uuid(foyer_resp.box_uuid);

	POSEIDON_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::Rcnts::view("Connection has been shut down"));
	CIRCE_LOG_DEBUG("Established Websocket_connection: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	m_auth_token = STD_MOVE_IDN(auth_resp.auth_token);
} catch(...){
	Ip_ban_list::accumulate_auth_failure(get_remote_info().ip());
	throw;
}

bool Client_websocket_session::on_low_level_message_end(boost::uint64_t whole_size){
	POSEIDON_PROFILE_ME;

	const AUTO(exempt_private, get_config<bool>("client_generic_exempt_private_addresses", true));
	if(exempt_private && Poseidon::Sock_addr(get_remote_info()).is_private()){
		CIRCE_LOG_DEBUG("Client exempted: ", get_remote_info());
	} else {
		Ip_ban_list::accumulate_websocket_request(get_remote_info().ip());
	}

	return Poseidon::Websocket::Session::on_low_level_message_end(whole_size);
}

void Client_websocket_session::on_sync_data_message(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload){
	POSEIDON_PROFILE_ME;
	CIRCE_LOG_DEBUG("Received WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	const Poseidon::Mutex::Unique_lock lock(m_delivery_mutex);
	if(!m_delivery_job_spare){
		m_delivery_job_spare = boost::make_shared<Delivery_job>(virtual_shared_from_this<Client_websocket_session>());
		m_delivery_job_active = false;
	}
	if(!m_delivery_job_active){
		Poseidon::enqueue(m_delivery_job_spare);
		m_delivery_job_active = true;
	}
	// Enqueue the message if the new fiber has been created successfully.
	const AUTO(max_requests_pending, get_config<boost::uint64_t>("client_websocket_max_requests_pending", 100));
	POSEIDON_THROW_UNLESS(m_messages_pending.size() < max_requests_pending, Poseidon::Websocket::Exception, Poseidon::Websocket::status_going_away, Poseidon::Rcnts::view("Max number of requests pending exceeded"));
	CIRCE_LOG_TRACE("Enqueueing message: opcode = ", opcode, ", payload.size() = ", payload.size());
	m_messages_pending.emplace_back(opcode, STD_MOVE(payload));
}
void Client_websocket_session::on_sync_control_message(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload){
	POSEIDON_PROFILE_ME;
	CIRCE_LOG_TRACE("Received WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	if(opcode == Poseidon::Websocket::opcode_close){
		char data[125];
		std::size_t size = payload.peek(data, sizeof(data));
		boost::uint16_t temp16;
		if(size == 0){
			temp16 = Poseidon::Websocket::status_normal_closure;
			m_closure_reason = std::make_pair(temp16, "");
		} else if(size < 2){
			temp16 = Poseidon::Websocket::status_inacceptable;
			m_closure_reason = std::make_pair(temp16, "No enough bytes in WebSocket closure frame");
		} else {
			std::memcpy(&temp16, data, 2);
			m_closure_reason = std::make_pair(Poseidon::load_be(temp16), std::string(data + 2, size - 2));
		}
	}

	if(opcode == Poseidon::Websocket::opcode_pong){
		CIRCE_LOG_TRACE("Received PONG from client: remote = ", get_remote_info(), ", payload = ", payload);
	}

	Poseidon::Websocket::Session::on_sync_control_message(opcode, STD_MOVE(payload));
}

bool Client_websocket_session::has_been_shutdown() const NOEXCEPT {
	POSEIDON_PROFILE_ME;

	return Poseidon::Websocket::Session::has_been_shutdown_write();
}
bool Client_websocket_session::shutdown(Poseidon::Websocket::Status_code status_code, const char *reason) NOEXCEPT {
	POSEIDON_PROFILE_ME;
	CIRCE_LOG_DEBUG("Shutting down WebSocket connection: remote = ", get_remote_info(), ", status_code = ", status_code, ", reason = ", reason);

	return Poseidon::Websocket::Session::shutdown(status_code, reason);
}

bool Client_websocket_session::send(Poseidon::Websocket::Op_code opcode, Poseidon::Stream_buffer payload){
	POSEIDON_PROFILE_ME;
	CIRCE_LOG_TRACE("Sending WebSocket message: remote = ", get_remote_info(), ", opcode = ", opcode, ", payload.size() = ", payload.size());

	return Poseidon::Websocket::Session::send(opcode, STD_MOVE(payload), false);
}

}
}
