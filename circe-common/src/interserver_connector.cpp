// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

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
	const boost::weak_ptr<InterserverConnector> m_weak_connector;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_deflated_payload;

public:
	InterserverClient(const Poseidon::SockAddr &sock_addr, const boost::shared_ptr<InterserverConnector> &connector)
		: Poseidon::Cbpp::LowLevelClient(sock_addr, false), InterserverConnection(connector->m_application_key)
		, m_weak_connector(connector)
	{
		LOG_CIRCE_INFO("InterserverClient constructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}
	~InterserverClient() OVERRIDE {
		LOG_CIRCE_INFO("InterserverClient destructor: remote = ", Poseidon::Cbpp::LowLevelClient::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::LowLevelClient
	void on_low_level_data_message_header(boost::uint16_t message_id, boost::uint64_t payload_size) FINAL {
		const std::size_t max_message_size = InterserverConnection::get_max_message_size();
		DEBUG_THROW_UNLESS(payload_size <= max_message_size, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_REQUEST_TOO_LARGE, Poseidon::sslit("Message is too large"));
		m_magic_number = message_id;
		m_deflated_payload.clear();
	}
	void on_low_level_data_message_payload(boost::uint64_t /*payload_offset*/, Poseidon::StreamBuffer payload) FINAL {
		m_deflated_payload.splice(payload);
	}
	bool on_low_level_data_message_end(boost::uint64_t /*payload_size*/) FINAL {
		InterserverConnection::layer5_on_receive_data(m_magic_number, STD_MOVE(m_deflated_payload));
		return true;
	}
	bool on_low_level_control_message(Poseidon::Cbpp::StatusCode status_code, Poseidon::StreamBuffer param) FINAL {
		InterserverConnection::layer5_on_receive_control(status_code, STD_MOVE(param));
		return true;
	}
	void on_close(int err_code) FINAL {
		Poseidon::Cbpp::LowLevelClient::on_close(err_code);
		InterserverConnection::layer4_on_close();
	}

	// InterserverConnection
	const Poseidon::IpPort &layer5_get_remote_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::LowLevelClient::get_remote_info();
	}
	const Poseidon::IpPort &layer5_get_local_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::LowLevelClient::get_local_info();
	}
	bool layer5_has_been_shutdown() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::LowLevelClient::has_been_shutdown_write();
	}
	bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT FINAL {
		return Poseidon::Cbpp::LowLevelClient::shutdown(err_code, err_msg);
	}
	void layer4_force_shutdown() NOEXCEPT FINAL {
		return Poseidon::TcpSessionBase::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload) FINAL {
		Poseidon::Cbpp::LowLevelClient::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::StreamBuffer param) FINAL {
		Poseidon::Cbpp::LowLevelClient::send_control(status_code, STD_MOVE(param));
	}
	void layer7_post_set_connection_uuid() FINAL {
		// There is nothing to do.
	}
	CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) FINAL {
		PROFILE_ME;

		const AUTO(connector, m_weak_connector.lock());
		DEBUG_THROW_UNLESS(connector, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_GONE_AWAY, Poseidon::sslit("The server has been shut down"));

		LOG_CIRCE_DEBUG("Dispatching: typeid(*this).name() = ", typeid(*this).name(), ", message_id = ", message_id);
		const AUTO(servlet, connector->sync_get_servlet(message_id));
		DEBUG_THROW_UNLESS(servlet, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_NOT_FOUND, Poseidon::sslit("message_id not handled"));
		return (*servlet)(virtual_shared_from_this<InterserverClient>(), message_id, STD_MOVE(payload));
	}
};

void InterserverConnector::timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector){
	PROFILE_ME;

	const AUTO(connector, weak_connector.lock());
	if(!connector){
		return;
	}

	Poseidon::Mutex::UniqueLock lock(connector->m_mutex);
	for(AUTO(host_it, connector->m_hosts.begin()); host_it != connector->m_hosts.end(); ++host_it){
		AUTO(client, connector->m_weak_clients[*host_it].lock());
		if(!client){
			const AUTO(promised_sock_addr, Poseidon::DnsDaemon::enqueue_for_looking_up(*host_it, connector->m_port));
			lock.unlock();
			{
				LOG_CIRCE_INFO("<<< Looking up: ", *host_it);
				Poseidon::yield(promised_sock_addr);
				LOG_CIRCE_INFO(">>> Creating InterserverClient to ", Poseidon::IpPort(promised_sock_addr->get()));
			}
			lock.lock();
			client = connector->m_weak_clients[*host_it].lock();
			if(client){
				LOG_CIRCE_WARNING("Killing old client: remote = ", client->layer5_get_remote_info());
				client->Poseidon::TcpSessionBase::force_shutdown();
			}
			client = boost::make_shared<InterserverClient>(promised_sock_addr->get(), connector);
			client->set_no_delay();
			Poseidon::EpollDaemon::add_socket(client, true);
			connector->m_weak_clients[*host_it] = client;
			client->layer7_client_say_hello(VAL_INIT); // TODO: add options.
		}
		if(client->has_authenticated()){
			const boost::uint64_t local_now = Poseidon::get_local_time();
			char str[256];
			std::size_t len = Poseidon::format_time(str, sizeof(str), local_now, true);
			client->Poseidon::Cbpp::LowLevelClient::send_control(Poseidon::Cbpp::ST_PING, Poseidon::StreamBuffer(str, len));
		}
	}
}

InterserverConnector::InterserverConnector(boost::container::vector<std::string> hosts, boost::uint16_t port, std::string application_key)
	: m_hosts(STD_MOVE(hosts)), m_port(port), m_application_key(STD_MOVE(application_key))
{
	DEBUG_THROW_UNLESS(!m_hosts.empty(), Poseidon::Exception, Poseidon::sslit("No host to connect to"));
	DEBUG_THROW_UNLESS(m_port != 0, Poseidon::Exception, Poseidon::sslit("Port number is zero"));
	LOG_CIRCE_INFO("InterserverConnector constructor: hosts:port = ", Poseidon::implode(',', m_hosts), ':', m_port);
}
InterserverConnector::~InterserverConnector(){
	LOG_CIRCE_INFO("InterserverConnector destructor: hosts:port = ", Poseidon::implode(',', m_hosts), ':', m_port);
	clear(Poseidon::Cbpp::ST_GONE_AWAY);
}

void InterserverConnector::activate(){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_timer, Poseidon::Exception, Poseidon::sslit("InterserverConnector is already activated"));
	const AUTO(timer, Poseidon::TimerDaemon::register_timer(0, 5000, boost::bind(&timer_proc, virtual_weak_from_this<InterserverConnector>())));
	m_timer = timer;
}

boost::shared_ptr<InterserverConnection> InterserverConnector::get_client(const Poseidon::Uuid &connection_uuid) const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(client && (client->get_connection_uuid() == connection_uuid)){
			return client;
		}
	}
	return VAL_INIT;
}
std::size_t InterserverConnector::get_all_clients(boost::container::vector<boost::shared_ptr<InterserverConnection> > &clients_ret) const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	std::size_t count_added = 0;
	clients_ret.reserve(clients_ret.size() + m_weak_clients.size());
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(!client){
			continue;
		}
		clients_ret.emplace_back(STD_MOVE(client));
		++count_added;
	}
	return count_added;
}
std::size_t InterserverConnector::safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	std::size_t count_notified = 0;
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(!client){
			continue;
		}
		try {
			LOG_CIRCE_DEBUG("Sending notification to interserver client: remote = ", client->layer5_get_remote_info(), ": ", msg);
			client->send_notification(msg);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			client->layer5_shutdown(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
		}
		++count_notified;
	}
	return count_notified;
}
std::size_t InterserverConnector::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	std::size_t count_shutdown = 0;
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); it = m_weak_clients.erase(it)){
		AUTO(client, it->second.lock());
		if(!client){
			continue;
		}
		LOG_CIRCE_DEBUG("Disconnecting interserver client: remote = ", client->layer5_get_remote_info());
		client->layer5_shutdown(err_code, err_msg);
		++count_shutdown;
	}
	return count_shutdown;
}

}
}
