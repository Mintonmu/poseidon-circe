// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/auth_acceptor.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Auth::AuthAcceptor::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET_FOR(const boost::shared_ptr<Common::InterserverConnection> &connection, Protocol::GA_ClientHttpAuthenticationRequest req){
	LOG_POSEIDON_FATAL("TODO: ", req);

	Protocol::AG_ClientHttpAuthenticationResponse resp;
	return resp;
}

DEFINE_SERVLET_FOR(const boost::shared_ptr<Common::InterserverConnection> &connection, Protocol::GA_ClientWebSocketAuthenticationRequest req){
	LOG_POSEIDON_FATAL("TODO: ", req);

	Protocol::AG_ClientWebSocketAuthenticationResponse resp;
	return resp;
}

}
}
