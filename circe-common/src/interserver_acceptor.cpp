// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_acceptor.hpp"
#include "interserver_connection.hpp"
#include "cbpp_response.hpp"
#include "mmain.hpp"
#include <poseidon/singletons/main_config.hpp>
#include <poseidon/cbpp/low_level_session.hpp>
#include <poseidon/cbpp/exception.hpp>
#include <poseidon/singletons/epoll_daemon.hpp>

namespace Circe {
namespace Common {

class InterserverAcceptor::InterserverSession : public Poseidon::Cbpp::LowLevelSession, public InterserverConnection {
private:
	const boost::weak_ptr<InterserverAcceptor> m_weak_parent;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_deflated_payload;

public:
	InterserverSession(Poseidon::Move<Poseidon::UniqueFile> socket, const boost::shared_ptr<InterserverAcceptor> &parent)
		: Poseidon::Cbpp::LowLevelSession(STD_MOVE(socket)), InterserverConnection(parent->m_application_key)
		, m_weak_parent(parent)
	{
		LOG_CIRCE_INFO("InterserverSession constructor: remote = ", Poseidon::Cbpp::LowLevelSession::get_remote_info());
	}
	~InterserverSession(){
		LOG_CIRCE_INFO("InterserverSession destructor: remote = ", Poseidon::Cbpp::LowLevelSession::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::LowLevelSession
	void on_low_level_data_message_header(boost::uint16_t message_id, boost::uint64_t payload_size) OVERRIDE {
		const AUTO(max_message_size, get_config<std::size_t>("interserver_max_message_size", 1048576));
		DEBUG_THROW_UNLESS(payload_size <= max_message_size, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_REQUEST_TOO_LARGE, Poseidon::sslit("Message is too large"));
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
	bool layer5_shutdown() NOEXCEPT OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::shutdown_read();
		return Poseidon::Cbpp::LowLevelSession::shutdown_write();
	}
	void layer4_force_shutdown() NOEXCEPT OVERRIDE {
		return Poseidon::TcpSessionBase::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload) OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::StreamBuffer param) OVERRIDE {
		Poseidon::Cbpp::LowLevelSession::send_status(status_code, STD_MOVE(param));
	}
	CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) OVERRIDE {
		PROFILE_ME;

		const AUTO(parent, m_weak_parent.lock());
		DEBUG_THROW_UNLESS(parent, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_GONE_AWAY, Poseidon::sslit("The server has been shut down"));
		const AUTO(servlet, parent->get_servlet(message_id));
		DEBUG_THROW_UNLESS(servlet, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_NOT_FOUND, Poseidon::sslit("message_id not handled"));
		AUTO(resp, (*servlet)(virtual_shared_from_this<InterserverSession>(), message_id, STD_MOVE(payload)));
		reset_timeout();
		return resp;
	}

public:
	void reset_timeout(){
		const AUTO(timeout, Poseidon::MainConfig::get<boost::uint64_t>("cbpp_keep_alive_timeout", 30000));
		set_timeout(timeout);
	}
};

InterserverAcceptor::InterserverAcceptor(const char *bind, unsigned port, const char *cert, const char *pkey, std::string application_key)
	: Poseidon::TcpServerBase(Poseidon::IpPort(bind, port), cert, pkey)
	, m_application_key(STD_MOVE(application_key))
{
	LOG_CIRCE_INFO("InterserverAcceptor constructor: local = ", get_local_info());
}
InterserverAcceptor::~InterserverAcceptor(){
	LOG_CIRCE_INFO("InterserverAcceptor destructor: local = ", get_local_info());
	clear(Poseidon::Cbpp::ST_GONE_AWAY);
}

boost::shared_ptr<Poseidon::TcpSessionBase> InterserverAcceptor::on_client_connect(Poseidon::Move<Poseidon::UniqueFile> socket){
	PROFILE_ME;

	AUTO(session, boost::make_shared<InterserverSession>(STD_MOVE(socket), virtual_shared_from_this<InterserverAcceptor>()));
	session->set_no_delay();
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		bool erase_it;
		for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); erase_it ? (it = m_weak_sessions.erase(it)) : ++it){
			erase_it = it->second.expired();
		}
		DEBUG_THROW_UNLESS(m_weak_sessions.emplace(session->get_connection_uuid(), session).second, Poseidon::Exception, Poseidon::sslit("Duplicate InterserverSession UUID"));
	}
	return STD_MOVE_IDN(session);
}

void InterserverAcceptor::activate(){
	PROFILE_ME;

	Poseidon::EpollDaemon::add_socket(virtual_shared_from_this<InterserverAcceptor>(), true);
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
void InterserverAcceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	VALUE_TYPE(m_weak_sessions) weak_sessions;
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		weak_sessions.swap(m_weak_sessions);
	}
	for(AUTO(it, weak_sessions.begin()); it != weak_sessions.end(); ++it){
		const AUTO(session, it->second.lock());
		if(session){
			LOG_CIRCE_DEBUG("Disconnecting session: remote = ", session->Poseidon::Cbpp::LowLevelSession::get_remote_info());
			session->Poseidon::Cbpp::LowLevelSession::shutdown(err_code, err_msg);
		}
	}
}

}
}
