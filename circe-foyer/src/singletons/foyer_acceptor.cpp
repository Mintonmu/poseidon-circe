// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "foyer_acceptor.hpp"
#include "servlet_container.hpp"
#include "common/interserver_acceptor.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Foyer {

namespace {
	class SpecializedAcceptor : public Common::InterserverAcceptor {
	public:
		SpecializedAcceptor(std::string bind, unsigned port, std::string application_key)
			: Common::InterserverAcceptor(STD_MOVE(bind), port, STD_MOVE(application_key))
		{ }

	protected:
		boost::shared_ptr<const Common::InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return ServletContainer::get_servlet(message_id);
		}
	};

	boost::weak_ptr<SpecializedAcceptor> g_weak_acceptor;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	PROFILE_ME;

	const AUTO(bind, get_config<std::string>("foyer_acceptor_bind", "0.0.0.0"));
	const AUTO(port, get_config<boost::uint16_t>("foyer_acceptor_port", 10812));
	const AUTO(appkey, get_config<std::string>("foyer_acceptor_appkey", "testkey"));
	LOG_CIRCE_INFO("Initializing FoyerAcceptor...");
	const AUTO(acceptor, boost::make_shared<SpecializedAcceptor>(bind, port, appkey));
	acceptor->activate();
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

boost::shared_ptr<Common::InterserverConnection> FoyerAcceptor::get_session(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return VAL_INIT;
	}
	return acceptor->get_session(connection_uuid);
}
void FoyerAcceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return;
	}
	return acceptor->clear(err_code, err_msg);
}
void FoyerAcceptor::safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return;
	}
	return acceptor->safe_broadcast_notification(msg);
}

}
}
