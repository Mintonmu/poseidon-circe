// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "auth_acceptor.hpp"
#include "servlet_container.hpp"
#include "common/interserver_acceptor.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Auth {

namespace {
	boost::weak_ptr<Common::InterserverAcceptor> g_weak_acceptor;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	PROFILE_ME;

	const AUTO(bind, get_config<std::string>("auth_acceptor_bind", "0.0.0.0"));
	const AUTO(port, get_config<boost::uint16_t>("auth_acceptor_port", 10818));
	const AUTO(appkey, get_config<std::string>("auth_acceptor_appkey", "testkey"));
	LOG_CIRCE_INFO("Initializing AuthAcceptor...");
	const AUTO(acceptor, boost::make_shared<Common::InterserverAcceptor>(ServletContainer::get_container(), bind, port, appkey));
	acceptor->activate();
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

boost::shared_ptr<Common::InterserverConnection> AuthAcceptor::get_session(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return VAL_INIT;
	}
	return acceptor->get_session(connection_uuid);
}
void AuthAcceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return;
	}
	return acceptor->clear(err_code, err_msg);
}

}
}
