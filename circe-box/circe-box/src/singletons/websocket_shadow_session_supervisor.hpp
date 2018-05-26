// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_BOX_SINGLETONS_WEBSOCKET_SHADOW_SESSION_SUPERVISOR_HPP_
#define CIRCE_BOX_SINGLETONS_WEBSOCKET_SHADOW_SESSION_SUPERVISOR_HPP_

#include "../websocket_shadow_session.hpp"

namespace Circe {
namespace Box {

class Websocket_shadow_session_supervisor {
private:
	Websocket_shadow_session_supervisor();

public:
	static boost::shared_ptr<Websocket_shadow_session> get_session(const Poseidon::Uuid &client_uuid);
	static std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Websocket_shadow_session> > &sessions_ret);
	static void attach_session(const boost::shared_ptr<Websocket_shadow_session> &session);
	static boost::shared_ptr<Websocket_shadow_session> detach_session(const Poseidon::Uuid &client_uuid) NOEXCEPT;
	static std::size_t clear(Poseidon::Websocket::Status_code status_code, const char *reason = "") NOEXCEPT;
};

}
}

#endif
