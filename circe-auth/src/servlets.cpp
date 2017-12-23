// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Auth::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &gate_conn, Protocol::GA_AuthenticateHttpRequest gate_req){
	LOG_POSEIDON_FATAL("TODO: CHECK AUTHENTICATION: ", gate_req);

	Protocol::AG_ReturnHttpAuthenticationResult gate_resp;
	return gate_resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &gate_conn, Protocol::GA_AuthenticateWebSocketConnection gate_req){
	LOG_POSEIDON_FATAL("TODO: CHECK AUTHENTICATION: ", gate_req);

	Protocol::AG_ReturnWebSocketAuthenticationResult gate_resp;
	return gate_resp;
}

}
}
