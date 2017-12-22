// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>

namespace Circe {
namespace Common {

class InterserverServletContainer;
class InterserverConnection;
class CbppResponse;

class InterserverAcceptor : public virtual Poseidon::VirtualSharedFromThis {
private:
	class InterserverSession;
	class InterserverServer;

private:
	const boost::weak_ptr<const InterserverServletContainer> m_weak_servlet_container;
	const std::string m_bind;
	const unsigned m_port;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<InterserverServer> m_server;
	boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<InterserverSession> > m_weak_sessions;

public:
	InterserverAcceptor(const boost::shared_ptr<const InterserverServletContainer> &servlet_container, std::string bind, unsigned port, std::string application_key);
	~InterserverAcceptor() OVERRIDE;

public:
	void activate();

	boost::shared_ptr<InterserverConnection> get_session(const Poseidon::Uuid &connection_uuid) const;
	void clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
