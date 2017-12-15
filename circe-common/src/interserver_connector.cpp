// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connector.hpp"
#include "interserver_connection.hpp"
#include "cbpp_response.hpp"
#include "mmain.hpp"
#include <poseidon/singletons/timer_daemon.hpp>
#include <poseidon/singletons/dns_daemon.hpp>
#include <poseidon/cbpp/low_level_client.hpp>
#include <poseidon/cbpp/exception.hpp>
#include <poseidon/sock_addr.hpp>
#include <poseidon/singletons/epoll_daemon.hpp>

namespace Circe {
namespace Common {

class InterserverConnector::InterserverClient : public Poseidon::Cbpp::LowLevelClient, public InterserverConnection {
private:
	const boost::weak_ptr<InterserverConnector> m_weak_parent;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_deflated_payload;

public:
	InterserverClient(const Poseidon::SockAddr &sock_addr, bool use_ssl, const boost::shared_ptr<InterserverConnector> &parent)
		: Poseidon::Cbpp::LowLevelClient(sock_addr, use_ssl), InterserverConnection(parent->m_application_key)
		, m_weak_parent(parent)
	{
		LOG_CIRCE_INFO("InterserverClient constructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}
	~InterserverClient(){
		LOG_CIRCE_INFO("InterserverClient destructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::LowLevelClient
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
		Poseidon::Cbpp::LowLevelClient::on_close(err_code);
		InterserverConnection::layer4_on_close();
	}

	// InterserverConnection
	const Poseidon::IpPort &layer5_get_remote_info() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelClient::get_remote_info();
	}
	const Poseidon::IpPort &layer5_get_local_info() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelClient::get_local_info();
	}
	bool layer5_has_been_shutdown() const NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelClient::has_been_shutdown_write();
	}
	bool layer5_shutdown() NOEXCEPT OVERRIDE {
		Poseidon::Cbpp::LowLevelClient::shutdown_read();
		return Poseidon::Cbpp::LowLevelClient::shutdown_write();
	}
	void layer4_force_shutdown() NOEXCEPT OVERRIDE {
		return Poseidon::TcpSessionBase::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload) OVERRIDE {
		Poseidon::Cbpp::LowLevelClient::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::StreamBuffer param) OVERRIDE {
		Poseidon::Cbpp::LowLevelClient::send_control(status_code, STD_MOVE(param));
	}
	CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) OVERRIDE {
		PROFILE_ME;

		const AUTO(parent, m_weak_parent.lock());
		DEBUG_THROW_UNLESS(parent, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_GONE_AWAY, Poseidon::sslit("The server has been shut down"));
		const AUTO(servlet, parent->get_servlet(message_id));
		DEBUG_THROW_UNLESS(servlet, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_NOT_FOUND, Poseidon::sslit("Message not recognized"));
		return (*servlet)(virtual_shared_from_this<InterserverClient>(), message_id, STD_MOVE(payload));
	}

public:
	void say_hello(){
		InterserverConnection::layer7_client_say_hello();
	}
};

void InterserverConnector::timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector){
	PROFILE_ME;

	const AUTO(connector, weak_connector.lock());
	if(!connector){
		return;
	}

	boost::shared_ptr<InterserverClient> client;
	{
		const Poseidon::Mutex::UniqueLock lock(connector->m_mutex);
		client = connector->m_weak_client.lock();
	}
	if(client){
		return;
	}
	const AUTO(promised_sock_addr, Poseidon::DnsDaemon::enqueue_for_looking_up(connector->m_host, connector->m_port));
	Poseidon::yield(promised_sock_addr);
	{
		const Poseidon::Mutex::UniqueLock lock(connector->m_mutex);
		client = connector->m_weak_client.lock();
		if(client){
			client->Poseidon::TcpSessionBase::force_shutdown();
		}
		client = boost::make_shared<InterserverClient>(promised_sock_addr->get(), connector->m_use_ssl, connector);
		Poseidon::EpollDaemon::add_socket(client, true);
		connector->m_weak_client = client;
	}
	client->set_no_delay();
	client->say_hello();
}

InterserverConnector::InterserverConnector(const char *host, unsigned port, bool use_ssl, std::string application_key)
	: m_host(host), m_port(port), m_use_ssl(use_ssl), m_application_key(STD_MOVE(application_key))
{
	LOG_CIRCE_INFO("InterserverConnector constructor: host:port = ", m_host, ":", m_port, ", use_ssl = ", m_use_ssl);
}
InterserverConnector::~InterserverConnector(){
	LOG_CIRCE_INFO("InterserverConnector constructor: host:port = ", m_host, ":", m_port, ", use_ssl = ", m_use_ssl);
}

void InterserverConnector::activate(){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_timer, Poseidon::Exception, Poseidon::sslit("InterserverConnector is already activated"));
	m_timer = Poseidon::TimerDaemon::register_timer(0, 5000, boost::bind(&timer_proc, virtual_weak_from_this<InterserverConnector>()));
}

}
}
