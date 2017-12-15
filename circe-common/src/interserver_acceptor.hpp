// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/ip_port.hpp>
#include <poseidon/mutex.hpp>
#include "interserver_servlet_container.hpp"

namespace Circe {
namespace Common {

class InterserverConnection;
class CbppResponse;

class InterserverAcceptor : public virtual Poseidon::VirtualSharedFromThis, public InterserverServletContainer, public Poseidon::TcpServerBase {
private:
	class InterserverSession;

private:
	const std::string m_application_key;

public:
	InterserverAcceptor(const char *bind, unsigned port, const char *cert, const char *pkey, std::string application_key);
	~InterserverAcceptor() OVERRIDE;

private:
	boost::shared_ptr<Poseidon::TcpSessionBase> on_client_connect(Poseidon::Move<Poseidon::UniqueFile> socket) OVERRIDE;

public:
	void activate();
};

}
}

#endif
