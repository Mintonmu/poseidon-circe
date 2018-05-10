// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_acceptor.hpp"
#include "interserver_connection.hpp"
#include "interserver_servlet_container.hpp"
#include "interserver_response.hpp"
#include "protocol/exception.hpp"
#include <poseidon/singletons/epoll_daemon.hpp>
#include <poseidon/cbpp/low_level_session.hpp>
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/singletons/main_config.hpp>

namespace Circe {
namespace Common {

class Interserver_acceptor::Interserver_session : public Poseidon::Cbpp::Low_level_session, public Interserver_connection {
	friend Interserver_acceptor;

private:
	const boost::weak_ptr<Interserver_acceptor> m_weak_acceptor;

	boost::uint16_t m_magic_number;
	Poseidon::Stream_buffer m_deflated_payload;

public:
	Interserver_session(Poseidon::Move<Poseidon::Unique_file> socket, const boost::shared_ptr<Interserver_acceptor> &acceptor)
		: Poseidon::Cbpp::Low_level_session(STD_MOVE(socket)), Interserver_connection(acceptor->m_application_key)
		, m_weak_acceptor(acceptor)
	{
		LOG_CIRCE_INFO("Interserver_session constructor: remote = ", Poseidon::Cbpp::Low_level_session::get_remote_info());
	}
	~Interserver_session() OVERRIDE {
		LOG_CIRCE_INFO("Interserver_session destructor: remote = ", Poseidon::Cbpp::Low_level_session::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::Low_level_session
	void on_low_level_data_message_header(boost::uint16_t message_id, boost::uint64_t payload_size) FINAL {
		const std::size_t max_message_size = Interserver_connection::get_max_message_size();
		CIRCE_PROTOCOL_THROW_UNLESS(payload_size <= max_message_size, Protocol::error_request_too_large, Poseidon::Rcnts::view("Message is too large"));
		m_magic_number = message_id;
		m_deflated_payload.clear();
	}
	void on_low_level_data_message_payload(boost::uint64_t /*payload_offset*/, Poseidon::Stream_buffer payload) FINAL {
		m_deflated_payload.splice(payload);
	}
	bool on_low_level_data_message_end(boost::uint64_t /*payload_size*/) FINAL {
		Interserver_connection::layer5_on_receive_data(m_magic_number, STD_MOVE(m_deflated_payload));
		return true;
	}
	bool on_low_level_control_message(Protocol::Error_code status_code, Poseidon::Stream_buffer param) FINAL {
		Interserver_connection::layer5_on_receive_control(status_code, STD_MOVE(param));
		return true;
	}
	void on_close(int err_code) FINAL {
		Poseidon::Cbpp::Low_level_session::on_close(err_code);
		Interserver_connection::layer4_on_close();
	}

	// Interserver_connection
	const Poseidon::Ip_port &layer5_get_remote_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_session::get_remote_info();
	}
	const Poseidon::Ip_port &layer5_get_local_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_session::get_local_info();
	}
	bool layer5_has_been_shutdown() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_session::has_been_shutdown_write();
	}
	bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_session::shutdown(err_code, err_msg);
	}
	void layer4_force_shutdown() NOEXCEPT FINAL {
		return Poseidon::Tcp_session_base::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::Stream_buffer deflated_payload) FINAL {
		Poseidon::Cbpp::Low_level_session::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::Stream_buffer param) FINAL {
		Poseidon::Cbpp::Low_level_session::send_status(status_code, STD_MOVE(param));
		set_timeout(Poseidon::Main_config::get<boost::uint64_t>("cbpp_keep_alive_timeout", 30000));
	}
	void layer7_post_set_connection_uuid() FINAL {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		CIRCE_PROTOCOL_THROW_UNLESS(acceptor, Protocol::error_gone_away, Poseidon::Rcnts::view("The server has been shut down"));

		const Poseidon::Mutex::Unique_lock lock(acceptor->m_mutex);
		bool erase_it;
		for(AUTO(it, acceptor->m_weak_sessions.begin()); it != acceptor->m_weak_sessions.end(); erase_it ? (it = acceptor->m_weak_sessions.erase(it)) : ++it){
			erase_it = it->second.expired();
		}
		const AUTO(pair, acceptor->m_weak_sessions.emplace(get_connection_uuid(), virtual_shared_from_this<Interserver_session>()));
		DEBUG_THROW_UNLESS(pair.second, Poseidon::Exception, Poseidon::Rcnts::view("Duplicate Interserver_session UUID"));
	}
	Interserver_response layer7_on_sync_message(boost::uint16_t message_id, Poseidon::Stream_buffer payload) FINAL {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		CIRCE_PROTOCOL_THROW_UNLESS(acceptor, Protocol::error_gone_away, Poseidon::Rcnts::view("The server has been shut down"));

		LOG_CIRCE_TRACE("Dispatching: typeid(*this).name() = ", typeid(*this).name(), ", message_id = ", message_id);
		const AUTO(servlet, acceptor->sync_get_servlet(message_id));
		CIRCE_PROTOCOL_THROW_UNLESS(servlet, Protocol::error_not_found, Poseidon::Rcnts::view("message_id not handled"));
		return (*servlet)(virtual_shared_from_this<Interserver_session>(), message_id, STD_MOVE(payload));
	}
};

class Interserver_acceptor::Interserver_server : public Poseidon::Tcp_server_base {
	friend Interserver_acceptor;

private:
	const boost::weak_ptr<Interserver_acceptor> m_weak_acceptor;

public:
	Interserver_server(const std::string &bind, boost::uint16_t port, const boost::shared_ptr<Interserver_acceptor> &acceptor)
		: Poseidon::Tcp_server_base(Poseidon::Ip_port(bind.c_str(), port), "", "")
		, m_weak_acceptor(acceptor)
	{
		LOG_CIRCE_INFO("Interserver_server constructor: local = ", Poseidon::Tcp_server_base::get_local_info());
	}
	~Interserver_server() FINAL {
		LOG_CIRCE_INFO("Interserver_server destructor: local = ", Poseidon::Tcp_server_base::get_local_info());
	}

protected:
	boost::shared_ptr<Poseidon::Tcp_session_base> on_client_connect(Poseidon::Move<Poseidon::Unique_file> socket) FINAL {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		DEBUG_THROW_UNLESS(acceptor, Poseidon::Exception, Poseidon::Rcnts::view("The server has been shut down"));

		const Poseidon::Mutex::Unique_lock lock(acceptor->m_mutex);
		AUTO(session, boost::make_shared<Interserver_session>(STD_MOVE(socket), acceptor));
		session->set_no_delay();
		return STD_MOVE_IDN(session);
	}
};

Interserver_acceptor::Interserver_acceptor(std::string bind, boost::uint16_t port, std::string application_key)
	: m_bind(STD_MOVE(bind)), m_port(port), m_application_key(STD_MOVE(application_key))
{
	DEBUG_THROW_UNLESS(!m_bind.empty(), Poseidon::Exception, Poseidon::Rcnts::view("No bind address specified"));
	DEBUG_THROW_UNLESS(m_port != 0, Poseidon::Exception, Poseidon::Rcnts::view("Port number is zero"));
	LOG_CIRCE_INFO("Interserver_acceptor constructor: bind:port = ", m_bind, ':', m_port);
}
Interserver_acceptor::~Interserver_acceptor(){
	LOG_CIRCE_INFO("Interserver_acceptor destructor: bind:port = ", m_bind, ':', m_port);
	clear(Protocol::error_gone_away);
}

void Interserver_acceptor::activate(){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_server, Poseidon::Exception, Poseidon::Rcnts::view("Interserver_acceptor is already activated"));
	const AUTO(server, boost::make_shared<Interserver_server>(m_bind.c_str(), m_port, virtual_shared_from_this<Interserver_acceptor>()));
	m_server = server;
	Poseidon::Epoll_daemon::add_socket(server, false);
}

boost::shared_ptr<Interserver_connection> Interserver_acceptor::get_session(const Poseidon::Uuid &connection_uuid) const {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	const AUTO(it, m_weak_sessions.find(connection_uuid));
	if(it != m_weak_sessions.end()){
		AUTO(session, it->second.lock());
		if(session){
			DEBUG_THROW_ASSERT(session->get_connection_uuid() == connection_uuid);
			return session;
		}
	}
	return VAL_INIT;
}
std::size_t Interserver_acceptor::get_all_sessions(boost::container::vector<boost::shared_ptr<Interserver_connection> > &sessions_ret) const {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	std::size_t count_added = 0;
	sessions_ret.reserve(sessions_ret.size() + m_weak_sessions.size());
	for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
		AUTO(session, it->second.lock());
		if(!session){
			continue;
		}
		sessions_ret.emplace_back(STD_MOVE_IDN(session));
		++count_added;
	}
	return count_added;
}
std::size_t Interserver_acceptor::safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	std::size_t count_notified = 0;
	for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
		AUTO(session, it->second.lock());
		if(!session){
			continue;
		}
		try {
			LOG_CIRCE_DEBUG("Broadcasting notification over Interserver_connection: remote = ", session->layer5_get_remote_info(), ": ", msg);
			session->send_notification(msg);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			session->layer5_shutdown(Protocol::error_internal_error, e.what());
		}
		++count_notified;
	}
	return count_notified;
}
std::size_t Interserver_acceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	std::size_t count_shutdown = 0;
	for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); it = m_weak_sessions.erase(it)){
		AUTO(session, it->second.lock());
		if(!session){
			continue;
		}
		LOG_CIRCE_DEBUG("Disconnecting Interserver_connection: remote = ", session->layer5_get_remote_info());
		session->layer5_shutdown(err_code, err_msg);
		++count_shutdown;
	}
	return count_shutdown;
}

}
}
