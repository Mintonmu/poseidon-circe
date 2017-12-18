// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_AUTH_SINGLETONS_AUTH_ACCEPTOR_HPP_
#define CIRCE_AUTH_SINGLETONS_AUTH_ACCEPTOR_HPP_

#include "common/interserver_connection.hpp"
#include "common/interserver_servlet_container.hpp"

namespace Circe {
namespace Auth {

class AuthAcceptor {
private:
	AuthAcceptor();

public:
	static void insert_servlet(boost::uint16_t message_id, Common::InterserverServletCallback callback);
	static bool remove_servlet(boost::uint16_t message_id) NOEXCEPT;

	static boost::shared_ptr<Common::InterserverConnection> get_session(const Poseidon::Uuid &connection_uuid);
	static void clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif