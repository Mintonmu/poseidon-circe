// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_BOX_SINGLETONS_WEBSOCKET_SHADOW_SESSION_SUPERVISOR_HPP_
#define CIRCE_BOX_SINGLETONS_WEBSOCKET_SHADOW_SESSION_SUPERVISOR_HPP_

#include "../websocket_shadow_session.hpp"

namespace Circe {
namespace Box {

class WebSocketShadowSessionSupervisor {
private:
	WebSocketShadowSessionSupervisor();

public:
	static boost::shared_ptr<WebSocketShadowSession> get_session(const Poseidon::Uuid &client_uuid);
	static std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<WebSocketShadowSession> > &sessions_ret);
	static void attach_session(const boost::shared_ptr<WebSocketShadowSession> &session);
	static boost::shared_ptr<WebSocketShadowSession> detach_session(const Poseidon::Uuid &client_uuid) NOEXCEPT;
	static std::size_t clear(Poseidon::WebSocket::StatusCode status_code, const char *reason = "") NOEXCEPT;
};

}
}

#endif
