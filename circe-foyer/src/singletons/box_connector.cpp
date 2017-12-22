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
	boost::weak_ptr<Common::InterserverConnector> g_weak_connector;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	PROFILE_ME;

	const AUTO(hosts, get_config_v<std::string>("box_connector_host"));
	const AUTO(port, get_config<boost::uint16_t>("box_connector_port", 10819));
	const AUTO(appkey, get_config<std::string>("box_connector_appkey", "testkey"));
	const AUTO(connector, boost::make_shared<Common::InterserverConnector>(ServletContainer::get_safe_container(), hosts, port, appkey));
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

}
}
