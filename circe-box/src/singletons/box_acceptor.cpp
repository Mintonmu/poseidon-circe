// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "box_acceptor.hpp"
#include "common/interserver_acceptor.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Box {

namespace {
	boost::weak_ptr<Common::InterserverAcceptor> g_weak_acceptor;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	PROFILE_ME;

	const AUTO(bind, get_config<std::string>("box_acceptor_bind", "0.0.0.0"));
	const AUTO(port, get_config<boost::uint16_t>("box_acceptor_port", 10819));
	const AUTO(appkey, get_config<std::string>("box_acceptor_appkey", "testkey"));
	LOG_CIRCE_INFO("Initializing BoxAcceptor...");
	const AUTO(acceptor, boost::make_shared<Common::InterserverAcceptor>(bind, port, appkey));
	acceptor->activate();
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

void BoxAcceptor::insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<Common::InterserverServletCallback> &servlet){
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("BoxAcceptor not initialized"));
	}
	return acceptor->insert_servlet(message_id, servlet);
}
bool BoxAcceptor::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return false;
	}
	return acceptor->remove_servlet(message_id);
}

boost::shared_ptr<Common::InterserverConnection> BoxAcceptor::get_session(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return VAL_INIT;
	}
	return acceptor->get_session(connection_uuid);
}
void BoxAcceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return;
	}
	return acceptor->clear(err_code, err_msg);
}

}
}
