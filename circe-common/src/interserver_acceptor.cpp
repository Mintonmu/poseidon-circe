// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_acceptor.hpp"
#include "interserver_connection.hpp"
#include "interserver_servlet_container.hpp"
#include "cbpp_response.hpp"
#include <poseidon/singletons/epoll_daemon.hpp>
#include <poseidon/cbpp/exception.hpp>
#include <poseidon/cbpp/low_level_session.hpp>
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/singletons/main_config.hpp>

namespace Circe {
namespace Common {

class InterserverAcceptor::InterserverSession : public Poseidon::Cbpp::LowLevelSession, public InterserverConnection {
	friend InterserverAcceptor;

private:
	const boost::weak_ptr<InterserverAcceptor> m_weak_acceptor;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_deflated_payload;

public:
	InterserverSession(Poseidon::Move<Poseidon::UniqueFile> socket, const boost::shared_ptr<InterserverAcceptor> &acceptor)
		: Poseidon::Cbpp::LowLevelSession(STD_MOVE(socket)), InterserverConnection(acceptor->m_application_key)
		, m_weak_acceptor(acceptor)
	{
		LOG_CIRCE_INFO("InterserverSession constructor: remote = ", Poseidon::Cbpp::LowLevelSession::get_remote_info());
	}
	~InterserverSession() OVERRIDE {
		LOG_CIRCE_INFO("InterserverSession destructor: remote = ", Poseidon::Cbpp::LowLevelSession::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::LowLevelSession
	void on_low_level_data_message_header(boost::uint16_t message_id, boost::uint64_t payload_size) OVERRIDE {
		const std::size_t max_message_size = InterserverConnection::get_max_message_size();
		DEBUG_THROW_UNLESS(payload_size <= max_message_size, Poseidon::Cbpp::Exception, Protocol::ERR_REQUEST_TOO_LARGE, Poseidon::sslit("Message is too large"));
		m_magic_number = message_id;
		m_deflated_payload.clear();
	}
	void on_low_level_data_message_payload(boost::uint64_t /*payload_offset*/, Poseidon::StreamBuffer payload) OVERRIDE {
		m_deflated_payload.splice(payload);
	}
	bool on_low_level_data_message_end(boost::uint64_t /*payload_size*/) OVERRIDE {
		InterserverConnection::layer5_on_receive_data(m_magic_number, STD_MOVE(m_deflated_payload));
		return true;
	}
	bool on_low_level_control_message(Poseidon::Cbpp::StatusCode status_code, Poseidon::StreamBuffer param) OVERRIDE {
		InterserverConnection::layer5_on_receive_control(status_code, STD_MOVE(param));
		return true;
	}
	void on_close(int err_code) OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::on_close(err_code);
		InterserverConnection::layer4_on_close();
	}

	// InterserverConnection
	const Poseidon::IpPort &layer5_get_remote_info() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelSession::get_remote_info();
	}
	const Poseidon::IpPort &layer5_get_local_info() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelSession::get_local_info();
	}
	bool layer5_has_been_shutdown() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelSession::has_been_shutdown_write();
	}
	bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelSession::shutdown(err_code, err_msg);
	}
	void layer4_force_shutdown() NOEXCEPT OVERRIDE {
		return Poseidon::TcpSessionBase::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload) OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::StreamBuffer param) OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::send_status(status_code, STD_MOVE(param));
		set_timeout(Poseidon::MainConfig::get<boost::uint64_t>("cbpp_keep_alive_timeout", 30000));
	}
	void layer7_post_set_connection_uuid() OVERRIDE {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		DEBUG_THROW_UNLESS(acceptor, Poseidon::Cbpp::Exception, Protocol::ERR_GONE_AWAY, Poseidon::sslit("The server has been shut down"));

		const Poseidon::Mutex::UniqueLock lock(acceptor->m_mutex);
		bool erase_it;
		for(AUTO(it, acceptor->m_weak_sessions.begin()); it != acceptor->m_weak_sessions.end(); erase_it ? (it = acceptor->m_weak_sessions.erase(it)) : ++it){
			erase_it = it->second.expired();
		}
		const AUTO(pair, acceptor->m_weak_sessions.emplace(get_connection_uuid(), virtual_shared_from_this<InterserverSession>()));
		DEBUG_THROW_UNLESS(pair.second, Poseidon::Exception, Poseidon::sslit("Duplicate InterserverSession UUID"));
		acceptor->m_weak_sessions_pending.erase(virtual_weak_from_this<InterserverSession>());
	}
	CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) OVERRIDE {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		DEBUG_THROW_UNLESS(acceptor, Poseidon::Cbpp::Exception, Protocol::ERR_GONE_AWAY, Poseidon::sslit("The server has been shut down"));

		LOG_CIRCE_DEBUG("Dispatching: typeid(*this).name() = ", typeid(*this).name(), ", message_id = ", message_id);
		const AUTO(servlet, acceptor->sync_get_servlet(message_id));
		DEBUG_THROW_UNLESS(servlet, Poseidon::Cbpp::Exception, Protocol::ERR_NOT_FOUND, Poseidon::sslit("message_id not handled"));
		return (*servlet)(virtual_shared_from_this<InterserverSession>(), message_id, STD_MOVE(payload));
	}
};

class InterserverAcceptor::InterserverServer : public Poseidon::TcpServerBase {
	friend InterserverAcceptor;

private:
	const boost::weak_ptr<InterserverAcceptor> m_weak_acceptor;

public:
	InterserverServer(const std::string &bind, boost::uint16_t port, const boost::shared_ptr<InterserverAcceptor> &acceptor)
		: Poseidon::TcpServerBase(Poseidon::IpPort(bind.c_str(), port), "", "")
		, m_weak_acceptor(acceptor)
	{
		LOG_CIRCE_INFO("InterserverServer constructor: local = ", Poseidon::TcpServerBase::get_local_info());
	}
	~InterserverServer() OVERRIDE {
		LOG_CIRCE_INFO("InterserverServer destructor: local = ", Poseidon::TcpServerBase::get_local_info());
	}

protected:
	boost::shared_ptr<Poseidon::TcpSessionBase> on_client_connect(Poseidon::Move<Poseidon::UniqueFile> socket) OVERRIDE {
		PROFILE_ME;

		const AUTO(acceptor, m_weak_acceptor.lock());
		DEBUG_THROW_UNLESS(acceptor, Poseidon::Exception, Poseidon::sslit("The server has been shut down"));

		const Poseidon::Mutex::UniqueLock lock(acceptor->m_mutex);
		AUTO(session, boost::make_shared<InterserverSession>(STD_MOVE(socket), acceptor));
		session->set_no_delay();
		acceptor->m_weak_sessions_pending.emplace(session);
		return STD_MOVE_IDN(session);
	}
};

InterserverAcceptor::InterserverAcceptor(std::string bind, unsigned port, std::string application_key)
	: m_bind(STD_MOVE(bind)), m_port(port), m_application_key(STD_MOVE(application_key))
{
	LOG_CIRCE_INFO("InterserverAcceptor constructor: bind:port = ", m_bind, ":", m_port);
}
InterserverAcceptor::~InterserverAcceptor(){
	LOG_CIRCE_INFO("InterserverAcceptor destructor: bind:port = ", m_bind, ":", m_port);
	clear(Protocol::ERR_GONE_AWAY);
}

void InterserverAcceptor::activate(){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_server, Poseidon::Exception, Poseidon::sslit("InterserverAcceptor is already activated"));
	const AUTO(server, boost::make_shared<InterserverServer>(m_bind.c_str(), m_port, virtual_shared_from_this<InterserverAcceptor>()));
	m_server = server;
	Poseidon::EpollDaemon::add_socket(server, false);
}

boost::shared_ptr<InterserverConnection> InterserverAcceptor::get_session(const Poseidon::Uuid &connection_uuid) const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	const AUTO(it, m_weak_sessions.find(connection_uuid));
	if(it == m_weak_sessions.end()){
		return VAL_INIT;
	}
	return it->second.lock();
}
void InterserverAcceptor::safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
		const AUTO(session, it->second.lock());
		if(session){
			try {
				LOG_CIRCE_DEBUG("Sending notification to interserver session: remote = ", session->layer5_get_remote_info(), ": ", msg);
				session->send_notification(msg);
			} catch(std::exception &e){
				LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
				session->layer5_shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
			}
		}
	}
}
void InterserverAcceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	while(!m_weak_sessions_pending.empty()){
		const AUTO(session, m_weak_sessions_pending.begin()->lock());
		if(session){
			LOG_CIRCE_DEBUG("Disconnecting interserver session: remote = ", session->layer5_get_remote_info());
			session->layer5_shutdown(err_code, err_msg);
		}
		m_weak_sessions_pending.erase(m_weak_sessions_pending.begin());
	}
	while(!m_weak_sessions.empty()){
		const AUTO(session, m_weak_sessions.begin()->second.lock());
		if(session){
			LOG_CIRCE_DEBUG("Disconnecting interserver session: remote = ", session->layer5_get_remote_info());
			session->layer5_shutdown(err_code, err_msg);
		}
		m_weak_sessions.erase(m_weak_sessions.begin());
	}
}

}
}
