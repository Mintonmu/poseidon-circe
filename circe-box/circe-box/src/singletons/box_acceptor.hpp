// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_BOX_SINGLETONS_BOX_ACCEPTOR_HPP_
#define CIRCE_BOX_SINGLETONS_BOX_ACCEPTOR_HPP_

#include "common/interserver_connection.hpp"

namespace Circe {
namespace Box {

class Box_acceptor {
private:
	Box_acceptor();

public:
	static boost::shared_ptr<Common::Interserver_connection> get_session(const Poseidon::Uuid &connection_uuid);
	static std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > &sessions_ret);
	static std::size_t safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) NOEXCEPT;
	static std::size_t clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
