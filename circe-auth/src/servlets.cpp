// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_auth.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Auth::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Auth::HttpAuthenticationRequest req){
	LOG_POSEIDON_FATAL("TODO: CHECK AUTHENTICATION: ", req);

	Protocol::Auth::HttpAuthenticationResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Auth::WebSocketAuthenticationRequest req){
	LOG_POSEIDON_FATAL("TODO: CHECK AUTHENTICATION: ", req);

	Protocol::Auth::WebSocketAuthenticationResponse resp;
	return resp;
}

}
}
