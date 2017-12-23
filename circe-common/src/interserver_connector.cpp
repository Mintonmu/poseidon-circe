// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connector.hpp"
#include "interserver_connection.hpp"
#include "interserver_servlet_container.hpp"
#include "cbpp_response.hpp"
#include <poseidon/singletons/epoll_daemon.hpp>
#include <poseidon/cbpp/exception.hpp>
#include <poseidon/cbpp/low_level_client.hpp>
#include <poseidon/singletons/timer_daemon.hpp>
#include <poseidon/singletons/dns_daemon.hpp>
#include <poseidon/sock_addr.hpp>

namespace Circe {
namespace Common {

class InterserverConnector::InterserverClient : public Poseidon::Cbpp::LowLevelClient, public InterserverConnection {
	friend InterserverConnector;

private:
	const boost::weak_ptr<InterserverConnector> m_weak_parent;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_deflated_payload;

public:
	InterserverClient(const Poseidon::SockAddr &sock_addr, const boost::shared_ptr<InterserverConnector> &parent)
		: Poseidon::Cbpp::LowLevelClient(sock_addr, false), InterserverConnection(parent->m_application_key)
		, m_weak_parent(parent)
	{
		LOG_CIRCE_INFO("InterserverClient constructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}
	~InterserverClient() OVERRIDE {
		LOG_CIRCE_INFO("InterserverClient destructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::LowLevelClient
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
	bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT OVERRIDE {
		return Poseidon::Cbpp::LowLevelClient::shutdown(err_code, err_msg);
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
	void layer7_post_set_connection_uuid() OVERRIDE {
		// There is nothing to do.
	}
	CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) OVERRIDE {
		PROFILE_ME;

		const AUTO(parent, m_weak_parent.lock());
		DEBUG_THROW_UNLESS(parent, Poseidon::Cbpp::Exception, Protocol::ERR_GONE_AWAY, Poseidon::sslit("The server has been shut down"));

		const AUTO(servlet, parent->sync_get_servlet(message_id));
		DEBUG_THROW_UNLESS(servlet, Poseidon::Cbpp::Exception, Protocol::ERR_NOT_FOUND, Poseidon::sslit("message_id not handled"));
		return (*servlet)(virtual_shared_from_this<InterserverClient>(), message_id, STD_MOVE(payload));
	}
};

void InterserverConnector::timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector){
	PROFILE_ME;

	const AUTO(connector, weak_connector.lock());
	if(!connector){
		return;
	}

	boost::container::vector<boost::shared_ptr<const Poseidon::PromiseContainer<Poseidon::SockAddr> > > dns_results;
	for(std::size_t slot = 0; slot < connector->m_hosts.size(); ++slot){
		// Get the list of clients to create.
		{
			const Poseidon::Mutex::UniqueLock lock(connector->m_mutex);
			connector->m_weak_clients.resize(connector->m_hosts.size());
			if(!connector->m_weak_clients.at(slot).expired()){
				continue;
			}
		}
		LOG_CIRCE_DEBUG("About to create InterserverConnection to ", connector->m_hosts.at(slot));

		dns_results.resize(connector->m_hosts.size());
		AUTO(promised_sock_addr, Poseidon::DnsDaemon::enqueue_for_looking_up(connector->m_hosts.at(slot), connector->m_port));
		dns_results.at(slot) = STD_MOVE_IDN(promised_sock_addr);
	}
	if(dns_results.empty()){
		return;
	}
	for(std::size_t slot = 0; slot < connector->m_hosts.size(); ++slot){
		// Create clients now.
		const AUTO_REF(promised_sock_addr, dns_results.at(slot));
		if(!promised_sock_addr){
			continue;
		}
		try {
			Poseidon::yield(promised_sock_addr);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("DNS failure: host = ", connector->m_hosts.at(slot), ", what = ", e.what());
			continue;
		}
		const AUTO_REF(sock_addr, promised_sock_addr->get());
		LOG_CIRCE_DEBUG("Creating InterserverConnection to ", Poseidon::IpPort(sock_addr));

		const Poseidon::Mutex::UniqueLock lock(connector->m_mutex);
		AUTO(client, connector->m_weak_clients.at(slot).lock());
		if(client){
			LOG_CIRCE_WARNING("Killing old client: remote = ", client->layer5_get_remote_info());
			client->Poseidon::TcpSessionBase::force_shutdown();
		}
		client = boost::make_shared<InterserverClient>(sock_addr, connector);
		client->set_no_delay();
		client->layer7_client_say_hello();
		connector->m_weak_clients.at(slot) = client;
		Poseidon::EpollDaemon::add_socket(client, true);
	}
}

InterserverConnector::InterserverConnector(std::vector<std::string> hosts, unsigned port, std::string application_key)
	: m_hosts(STD_MOVE(hosts)), m_port(port), m_application_key(STD_MOVE(application_key))
{
	LOG_CIRCE_INFO("InterserverConnector constructor: hosts:port = ", Poseidon::implode(',', m_hosts), ":", m_port);
}
InterserverConnector::~InterserverConnector(){
	LOG_CIRCE_INFO("InterserverConnector destructor: hosts:port = ", Poseidon::implode(',', m_hosts), ":", m_port);
	clear(Protocol::ERR_GONE_AWAY);
}

void InterserverConnector::activate(){
	PROFILE_ME;
	DEBUG_THROW_UNLESS(!m_hosts.empty(), Poseidon::Exception, Poseidon::sslit("No hosts to connect to"));

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_timer, Poseidon::Exception, Poseidon::sslit("InterserverConnector is already activated"));
	const AUTO(timer, Poseidon::TimerDaemon::register_timer(0, 5000, boost::bind(&timer_proc, virtual_weak_from_this<InterserverConnector>())));
	m_timer = timer;
}

boost::shared_ptr<InterserverConnection> InterserverConnector::get_client() const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	if(m_weak_clients.empty()){
		return VAL_INIT;
	}
	const std::size_t slot = Poseidon::random_uint32() % m_weak_clients.size();
	return m_weak_clients.at(slot).lock();
}
void InterserverConnector::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		const AUTO(client, it->lock());
		if(client){
			LOG_CIRCE_DEBUG("Disconnecting client: remote = ", client->layer5_get_remote_info());
			client->layer5_shutdown(err_code, err_msg);
		}
	}
}

}
}
