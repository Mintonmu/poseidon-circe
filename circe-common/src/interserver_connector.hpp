// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include <boost/function.hpp>
#include <boost/container/flat_map.hpp>

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverConnector : public virtual Poseidon::VirtualSharedFromThis {
private:
	class InterserverClient;

public:
	typedef boost::function<
		CbppResponse (const boost::shared_ptr<InterserverConnection> &connection, boost::uint16_t message_id, Poseidon::StreamBuffer payload)
		> ServletCallback;

private:
	static void on_low_level_timer(const boost::weak_ptr<InterserverConnector> &weak_connector);

private:
	const std::string m_host;
	const unsigned m_port;
	const bool m_use_ssl;

	mutable Poseidon::Mutex m_connection_mutex;
	boost::weak_ptr<InterserverClient> m_client;

	mutable Poseidon::Mutex m_servlet_mutex;
	boost::container::flat_map<boost::uint16_t, boost::shared_ptr<const ServletCallback> > m_servlet_map;

public:
	InterserverConnector(const char *host, unsigned port, bool use_ssl);
	~InterserverConnector() OVERRIDE;

public:
	void activate();

	boost::shared_ptr<const ServletCallback> get_servlet(boost::uint16_t message_id) const;
	void insert_servlet(boost::uint16_t message_id, ServletCallback callback);
	bool remove_servlet(boost::uint16_t message_id) NOEXCEPT;
};

}
}

#endif
