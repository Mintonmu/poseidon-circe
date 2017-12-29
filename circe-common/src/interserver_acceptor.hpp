// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include "interserver_servlet_callback.hpp"

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverAcceptor : public virtual Poseidon::VirtualSharedFromThis {
private:
	class InterserverSession;
	class InterserverServer;

private:
	const std::string m_bind;
	const boost::uint16_t m_port;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<InterserverServer> m_server;
	boost::container::flat_multiset<boost::weak_ptr<InterserverSession> > m_weak_sessions_pending;
	boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<InterserverSession> > m_weak_sessions;

public:
	InterserverAcceptor(std::string bind, boost::uint16_t port, std::string application_key);
	~InterserverAcceptor() OVERRIDE;

protected:
	virtual boost::shared_ptr<const InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const = 0;

public:
	void activate();

	boost::shared_ptr<InterserverConnection> get_session(const Poseidon::Uuid &connection_uuid) const;
	std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<InterserverConnection> > &sessions_ret) const;
	std::size_t safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) const NOEXCEPT;
	std::size_t clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
