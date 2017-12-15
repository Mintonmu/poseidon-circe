// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include "interserver_servlet_container.hpp"

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverConnector : public virtual Poseidon::VirtualSharedFromThis, public InterserverServletContainer {
private:
	class InterserverClient;

private:
	static void timer_proc(const boost::weak_ptr<InterserverConnector> &weak_connector);

private:
	const std::string m_host;
	const unsigned m_port;
	const bool m_use_ssl;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<Poseidon::Timer> m_timer;
	boost::weak_ptr<InterserverClient> m_weak_client;

public:
	InterserverConnector(const char *host, unsigned port, bool use_ssl, std::string application_key);
	~InterserverConnector() OVERRIDE;

public:
	boost::shared_ptr<InterserverConnection> get_client() const;
	void activate();
};

}
}

#endif
