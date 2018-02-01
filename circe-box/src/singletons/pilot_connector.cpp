// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "pilot_connector.hpp"
#include "servlet_container.hpp"
#include "common/interserver_connector.hpp"
#include "protocol/error_codes.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Box {

namespace {
	class SpecializedConnector : public Common::InterserverConnector {
	public:
		SpecializedConnector(boost::container::vector<std::string> hosts, boost::uint16_t port, std::string application_key)
			: Common::InterserverConnector(STD_MOVE(hosts), port, STD_MOVE(application_key))
		{
			//
		}

	protected:
		boost::shared_ptr<const Common::InterserverServletCallback> sync_get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return ServletContainer::get_servlet(message_id);
		}
	};

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<SpecializedConnector> g_weak_connector;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(hosts, get_config_all<std::string>("pilot_connector_host"));
	const AUTO(port, get_config<boost::uint16_t>("pilot_connector_port"));
	const AUTO(appkey, get_config<std::string>("pilot_connector_appkey"));
	const AUTO(connector, boost::make_shared<SpecializedConnector>(hosts, port, appkey));
	connector->activate();
	handles.push(connector);
	g_weak_connector = connector;
}

boost::shared_ptr<Common::InterserverConnection> PilotConnector::get_client(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("PilotConnector has not been initialized.");
		return VAL_INIT;
	}
	return connector->get_client(connection_uuid);
}
std::size_t PilotConnector::get_all_clients(boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > &clients_ret){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("PilotConnector has not been initialized.");
		return 0;
	}
	return connector->get_all_clients(clients_ret);
}
std::size_t PilotConnector::safe_broadcast_notification(const Poseidon::Cbpp::MessageBase &msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("PilotConnector has not been initialized.");
		return 0;
	}
	return connector->safe_broadcast_notification(msg);
}
std::size_t PilotConnector::clear(long err_code, const char *err_msg) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		LOG_CIRCE_WARNING("PilotConnector has not been initialized.");
		return 0;
	}
	return connector->clear(err_code, err_msg);
}

}
}
