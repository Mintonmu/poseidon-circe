// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include "interserver_servlet_callback.hpp"

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverConnector : public virtual Poseidon::VirtualSharedFromThis {
private:
	class InterserverClient;

private:
	static void timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector);

private:
	const std::vector<std::string> m_hosts;
	const unsigned m_port;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<Poseidon::Timer> m_timer;
	std::vector<boost::weak_ptr<InterserverClient> > m_weak_clients;

public:
	InterserverConnector(std::vector<std::string> hosts, unsigned port, std::string application_key);
	~InterserverConnector() OVERRIDE;

protected:
	virtual boost::shared_ptr<const InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const = 0;

public:
	void activate();

	boost::shared_ptr<InterserverConnection> get_client() const;
	void clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
