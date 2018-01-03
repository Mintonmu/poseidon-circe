// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include "interserver_servlet_callback.hpp"

namespace Circe {
namespace Common {

class InterserverConnection;
class InterserverResponse;

class InterserverConnector : public virtual Poseidon::VirtualSharedFromThis {
private:
	class InterserverClient;

private:
	static void timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector);

private:
	const boost::container::vector<std::string> m_hosts;
	const boost::uint16_t m_port;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<Poseidon::Timer> m_timer;
	boost::container::flat_map<std::string, boost::weak_ptr<InterserverClient> > m_weak_clients;

public:
	InterserverConnector(boost::container::vector<std::string> hosts, boost::uint16_t port, std::string application_key);
	~InterserverConnector() OVERRIDE;

protected:
	virtual boost::shared_ptr<const InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const = 0;

public:
	void activate();

	boost::shared_ptr<InterserverConnection> get_client(const Poseidon::Uuid &connection_uuid) const;
	std::size_t get_all_clients(boost::container::vector<boost::shared_ptr<InterserverConnection> > &clients_ret) const;
	std::size_t safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) const NOEXCEPT;
	std::size_t clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
