// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_SINGLETONS_PILOT_ACCEPTOR_HPP_
#define CIRCE_PILOT_SINGLETONS_PILOT_ACCEPTOR_HPP_

#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

class PilotAcceptor {
private:
	PilotAcceptor();

public:
	static boost::shared_ptr<Common::InterserverConnection> get_session(const Poseidon::Uuid &connection_uuid);
	static std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > &sessions_ret);
	static std::size_t safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) NOEXCEPT;
	static std::size_t clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
