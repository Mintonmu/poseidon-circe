// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connector.hpp"
#include "interserver_connection.hpp"
#include "interserver_servlet_container.hpp"
#include "interserver_response.hpp"
#include "protocol/exception.hpp"
#include <poseidon/singletons/epoll_daemon.hpp>
#include <poseidon/cbpp/low_level_client.hpp>
#include <poseidon/singletons/timer_daemon.hpp>
#include <poseidon/singletons/dns_daemon.hpp>
#include <poseidon/sock_addr.hpp>

namespace Circe {
namespace Common {

class Interserver_connector::Interserver_client : public Poseidon::Cbpp::Low_level_client, public Interserver_connection {
	friend Interserver_connector;

private:
	const boost::weak_ptr<Interserver_connector> m_weak_connector;

	boost::uint16_t m_magic_number;
	Poseidon::Stream_buffer m_deflated_payload;

public:
	Interserver_client(const Poseidon::Sock_addr &sock_addr, const boost::shared_ptr<Interserver_connector> &connector)
		: Poseidon::Cbpp::Low_level_client(sock_addr, false), Interserver_connection(connector->m_application_key)
		, m_weak_connector(connector)
	{
		LOG_CIRCE_INFO("Interserver_client constructor: remote = ", Poseidon::Cbpp::Low_level_client::get_remote_info());
	}
	~Interserver_client() OVERRIDE {
		LOG_CIRCE_INFO("Interserver_client destructor: remote = ", Poseidon::Cbpp::Low_level_client::get_remote_info());
	}

protected:
	// Poseidon::Cbpp::Low_level_client
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
		Poseidon::Cbpp::Low_level_client::on_close(err_code);
		Interserver_connection::layer4_on_close();
	}

	// Interserver_connection
	const Poseidon::Ip_port &layer5_get_remote_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_client::get_remote_info();
	}
	const Poseidon::Ip_port &layer5_get_local_info() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_client::get_local_info();
	}
	bool layer5_has_been_shutdown() const NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_client::has_been_shutdown_write();
	}
	bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT FINAL {
		return Poseidon::Cbpp::Low_level_client::shutdown(err_code, err_msg);
	}
	void layer4_force_shutdown() NOEXCEPT FINAL {
		return Poseidon::Tcp_session_base::force_shutdown();
	}
	void layer5_send_data(boost::uint16_t magic_number, Poseidon::Stream_buffer deflated_payload) FINAL {
		Poseidon::Cbpp::Low_level_client::send(magic_number, STD_MOVE(deflated_payload));
	}
	void layer5_send_control(long status_code, Poseidon::Stream_buffer param) FINAL {
		Poseidon::Cbpp::Low_level_client::send_control(status_code, STD_MOVE(param));
	}
	void layer7_post_set_connection_uuid() FINAL {
		// There is nothing to do.
	}
	Interserver_response layer7_on_sync_message(boost::uint16_t message_id, Poseidon::Stream_buffer payload) FINAL {
		PROFILE_ME;

		const AUTO(connector, m_weak_connector.lock());
		CIRCE_PROTOCOL_THROW_UNLESS(connector, Protocol::error_gone_away, Poseidon::Rcnts::view("The server has been shut down"));

		LOG_CIRCE_TRACE("Dispatching: typeid(*this).name() = ", typeid(*this).name(), ", message_id = ", message_id);
		const AUTO(servlet, connector->sync_get_servlet(message_id));
		CIRCE_PROTOCOL_THROW_UNLESS(servlet, Protocol::error_not_found, Poseidon::Rcnts::view("message_id not handled"));
		return (*servlet)(virtual_shared_from_this<Interserver_client>(), message_id, STD_MOVE(payload));
	}
};

void Interserver_connector::timer_proc(const boost::weak_ptr<Interserver_connector> &weak_connector){
	PROFILE_ME;

	const AUTO(connector, weak_connector.lock());
	if(!connector){
		return;
	}

	Poseidon::Mutex::Unique_lock lock(connector->m_mutex);
	for(AUTO(host_it, connector->m_hosts.begin()); host_it != connector->m_hosts.end(); ++host_it){
		AUTO(client, connector->m_weak_clients[*host_it].lock());
		if(!client){
			const AUTO(promised_sock_addr, Poseidon::Dns_daemon::enqueue_for_looking_up(*host_it, connector->m_port));
			lock.unlock();
			{
				LOG_CIRCE_DEBUG("Looking up: ", *host_it);
				Poseidon::yield(promised_sock_addr);
				LOG_CIRCE_INFO("Resolved server hostname: ", *host_it, ":", connector->m_port, " as ", Poseidon::Ip_port(promised_sock_addr->get()));
			}
			lock.lock();
			client = connector->m_weak_clients[*host_it].lock();
			if(client){
				LOG_CIRCE_WARNING("Killing old client: remote = ", client->layer5_get_remote_info());
				client->Poseidon::Tcp_session_base::force_shutdown();
			}
			client = boost::make_shared<Interserver_client>(promised_sock_addr->get(), connector);
			client->set_no_delay();
			Poseidon::Option_map opt;
			opt.set(Poseidon::Rcnts::view("meow"), "123");
			opt.set(Poseidon::Rcnts::view("hello"), "456");
			client->layer7_client_say_hello(opt); // TODO: add options.
			Poseidon::Epoll_daemon::add_socket(client, true);
			connector->m_weak_clients[*host_it] = client;
		}
		if(client->has_authenticated()){
			const boost::uint64_t local_now = Poseidon::get_local_time();
			char str[256];
			std::size_t len = Poseidon::format_time(str, sizeof(str), local_now, true);
			client->Poseidon::Cbpp::Low_level_client::send_control(Poseidon::Cbpp::status_ping, Poseidon::Stream_buffer(str, len));
		}
	}
}

Interserver_connector::Interserver_connector(boost::container::vector<std::string> hosts, boost::uint16_t port, std::string application_key)
	: m_hosts(STD_MOVE(hosts)), m_port(port), m_application_key(STD_MOVE(application_key))
{
	DEBUG_THROW_UNLESS(!m_hosts.empty(), Poseidon::Exception, Poseidon::Rcnts::view("No host to connect to"));
	DEBUG_THROW_UNLESS(m_port != 0, Poseidon::Exception, Poseidon::Rcnts::view("Port number is zero"));
	LOG_CIRCE_INFO("Interserver_connector constructor: hosts:port = ", Poseidon::implode(',', m_hosts), ':', m_port);
}
Interserver_connector::~Interserver_connector(){
	LOG_CIRCE_INFO("Interserver_connector destructor: hosts:port = ", Poseidon::implode(',', m_hosts), ':', m_port);
	clear(Protocol::error_gone_away);
}

void Interserver_connector::activate(){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_timer, Poseidon::Exception, Poseidon::Rcnts::view("Interserver_connector is already activated"));
	const AUTO(timer, Poseidon::Timer_daemon::register_timer(0, 5000, boost::bind(&timer_proc, virtual_weak_from_this<Interserver_connector>())));
	m_timer = timer;
}

boost::shared_ptr<Interserver_connection> Interserver_connector::get_client(const Poseidon::Uuid &connection_uuid) const {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(client && (client->get_connection_uuid() == connection_uuid)){
			return client;
		}
	}
	return VAL_INIT;
}
std::size_t Interserver_connector::get_all_clients(boost::container::vector<boost::shared_ptr<Interserver_connection> > &clients_ret) const {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	std::size_t count_added = 0;
	clients_ret.reserve(clients_ret.size() + m_weak_clients.size());
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(!client){
			continue;
		}
		clients_ret.emplace_back(STD_MOVE_IDN(client));
		++count_added;
	}
	return count_added;
}
std::size_t Interserver_connector::safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
	std::size_t count_notified = 0;
	for(AUTO(it, m_weak_clients.begin()); it != m_weak_clients.end(); ++it){
		AUTO(client, it->second.lock());
		if(!client){
			continue;
		}
		try {
			LOG_CIRCE_DEBUG("Broadcasting notification to interserver client: remote = ", client->layer5_get_remote_info(), ": ", msg);
			client->send_notification(msg);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			client->layer5_shutdown(Protocol::error_internal_error, e.what());
		}
		++count_notified;
	}
	return count_notified;
}
std::size_t Interserver_connector::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(m_mutex);
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
