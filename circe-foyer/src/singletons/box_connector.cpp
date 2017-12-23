// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "box_connector.hpp"
#include "servlet_container.hpp"
#include "common/interserver_connector.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Foyer {

namespace {
	class SpecializedConnector : public Common::InterserverConnector {
	public:
		SpecializedConnector(std::vector<std::string> hosts, unsigned port, std::string application_key)
			: Common::InterserverConnector(STD_MOVE(hosts), port, STD_MOVE(application_key))
		{ }

	protected:
		boost::shared_ptr<const Common::InterserverServletCallback> get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return ServletContainer::get_servlet(message_id);
		}
	};

	boost::weak_ptr<SpecializedConnector> g_weak_connector;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	PROFILE_ME;

	const AUTO(hosts, get_config_v<std::string>("box_connector_host"));
	const AUTO(port, get_config<boost::uint16_t>("box_connector_port", 10819));
	const AUTO(appkey, get_config<std::string>("box_connector_appkey", "testkey"));
	const AUTO(connector, boost::make_shared<SpecializedConnector>(hosts, port, appkey));
	connector->activate();
	handles.push(connector);
	g_weak_connector = connector;
}

boost::shared_ptr<Common::InterserverConnection> BoxConnector::get_connection(){
	PROFILE_ME;

	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("BoxConnector has not been initialized.");
		return VAL_INIT;
	}
	return connector->get_client();
}
void BoxConnector::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("BoxConnector has not been initialized.");
		return;
	}
	connector->clear(err_code, err_msg);
}

}
}
