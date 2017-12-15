// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/ip_port.hpp>
#include <poseidon/mutex.hpp>
#include <boost/function.hpp>
#include <boost/container/flat_map.hpp>

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverAcceptor : public Poseidon::TcpServerBase {
public:
	typedef boost::function<
		CbppResponse (const boost::shared_ptr<InterserverConnection> &connection, boost::uint16_t message_id, Poseidon::StreamBuffer payload)
		> ServletCallback;

private:
	mutable Poseidon::Mutex m_servlet_mutex;
	boost::container::flat_map<boost::uint16_t, boost::shared_ptr<const ServletCallback> > m_servlet_map;

public:
	InterserverAcceptor(const Poseidon::IpPort &ip_port, const char *cert, const char *pkey);
	~InterserverAcceptor() OVERRIDE;

protected:
	boost::shared_ptr<Poseidon::TcpSessionBase> on_client_connect(Poseidon::Move<Poseidon::UniqueFile> socket) OVERRIDE;

public:
	boost::shared_ptr<const ServletCallback> get_servlet(boost::uint16_t message_id) const;
	void insert_servlet(boost::uint16_t message_id, ServletCallback callback);
	bool remove_servlet(boost::uint16_t message_id) NOEXCEPT;
};

}
}

#endif
