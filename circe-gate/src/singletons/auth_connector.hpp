// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_SINGLETONS_AUTH_CONNECTOR_HPP_
#define CIRCE_GATE_SINGLETONS_AUTH_CONNECTOR_HPP_

#include "common/interserver_connection.hpp"

namespace Circe {
namespace Gate {

class AuthConnector {
private:
	AuthConnector();

public:
	static boost::shared_ptr<Common::InterserverConnection> get_client();
	static void safe_send_notification(const Poseidon::Cbpp::MessageBase &msg) NOEXCEPT;
	static void clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
