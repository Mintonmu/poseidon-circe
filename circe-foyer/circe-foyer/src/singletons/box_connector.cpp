// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "box_connector.hpp"
#include "servlet_container.hpp"
#include "common/interserver_connector.hpp"
#include "protocol/error_codes.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Foyer {

namespace {
	class Specialized_connector : public Common::Interserver_connector {
	public:
		Specialized_connector(boost::container::vector<std::string> hosts, boost::uint16_t port, std::string application_key)
			: Common::Interserver_connector(STD_MOVE(hosts), port, STD_MOVE(application_key))
		{ }

	protected:
		boost::shared_ptr<const Common::Interserver_servlet_callback> sync_get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return Servlet_container::get_servlet(message_id);
		}
	};

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Specialized_connector> g_weak_connector;
}

POSEIDON_MODULE_RAII_PRIORITY(handles, Poseidon::module_init_priority_low){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(hosts, get_config_all<std::string>("box_connector_host"));
	const AUTO(port, get_config<boost::uint16_t>("box_connector_port"));
	const AUTO(appkey, get_config<std::string>("box_connector_appkey"));
	const AUTO(connector, boost::make_shared<Specialized_connector>(hosts, port, appkey));
	connector->activate();
	handles.push(connector);
	g_weak_connector = connector;
}

boost::shared_ptr<Common::Interserver_connection> Box_connector::get_client(const Poseidon::Uuid &connection_uuid){
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		CIRCE_LOG_WARNING("Box_connector has not been initialized.");
		return VAL_INIT;
	}
	return connector->get_client(connection_uuid);
}
std::size_t Box_connector::get_all_clients(boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > &clients_ret){
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		CIRCE_LOG_WARNING("Box_connector has not been initialized.");
		return 0;
	}
	return connector->get_all_clients(clients_ret);
}
std::size_t Box_connector::safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		CIRCE_LOG_WARNING("Box_connector has not been initialized.");
		return 0;
	}
	return connector->safe_broadcast_notification(msg);
}
std::size_t Box_connector::clear(long err_code, const char *err_msg) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(connector, g_weak_connector.lock());
	if(!connector){
		CIRCE_LOG_WARNING("Box_connector has not been initialized.");
		return 0;
	}
	return connector->clear(err_code, err_msg);
}

}
}
