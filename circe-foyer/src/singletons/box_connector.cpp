// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "box_connector.hpp"
#include "servlet_container.hpp"
#include "common/interserver_connector.hpp"
#include "protocol/error_codes.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Foyer {

namespace {
	class SpecializedConnector : public Common::InterserverConnector {
	public:
		SpecializedConnector(std::string host, boost::uint16_t port, std::string application_key)
			: Common::InterserverConnector(STD_MOVE(host), port, STD_MOVE(application_key))
		{ }

	protected:
		boost::shared_ptr<const Common::InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return ServletContainer::get_servlet(message_id);
		}
	};

	boost::weak_ptr<SpecializedConnector> g_weak_connector;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	PROFILE_ME;

	const AUTO(host, get_config<std::string>("box_connector_host", "localhost"));
	const AUTO(port, get_config<boost::uint16_t>("box_connector_port", 10819));
	const AUTO(appkey, get_config<std::string>("box_connector_appkey", "testkey"));
	const AUTO(connector, boost::make_shared<SpecializedConnector>(host, port, appkey));
	connector->activate();
	handles.push(connector);
	g_weak_connector = connector;
}

boost::shared_ptr<Common::InterserverConnection> BoxConnector::get_client(){
	PROFILE_ME;

	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("BoxConnector has not been initialized.");
		return VAL_INIT;
	}
	return connector->get_client();
}
void BoxConnector::safe_send_notification(const Poseidon::Cbpp::MessageBase &msg) NOEXCEPT {
	PROFILE_ME;

	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("BoxConnector has not been initialized.");
		return;
	}
	return connector->safe_send_notification(msg);
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
