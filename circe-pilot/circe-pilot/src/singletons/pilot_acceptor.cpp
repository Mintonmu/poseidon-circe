// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "pilot_acceptor.hpp"
#include "servlet_container.hpp"
#include "common/interserver_acceptor.hpp"
#include "protocol/error_codes.hpp"
#include "../mmain.hpp"

namespace Circe {
namespace Pilot {

namespace {
	class Specialized_acceptor : public Common::Interserver_acceptor {
	public:
		Specialized_acceptor(std::string bind, boost::uint16_t port, std::string application_key)
			: Common::Interserver_acceptor(STD_MOVE(bind), port, STD_MOVE(application_key))
		{ }

	protected:
		boost::shared_ptr<const Common::Interserver_servlet_callback> sync_get_servlet(boost::uint16_t message_id) const OVERRIDE {
			return Servlet_container::get_servlet(message_id);
		}
	};
	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Specialized_acceptor> g_weak_acceptor;
}

POSEIDON_MODULE_RAII_PRIORITY(handles, Poseidon::module_init_priority_low){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(bind, get_config<std::string>("pilot_acceptor_bind"));
	const AUTO(port, get_config<boost::uint16_t>("pilot_acceptor_port"));
	const AUTO(appkey, get_config<std::string>("pilot_acceptor_appkey"));
	CIRCE_LOG_INFO("Initializing Pilot_acceptor...");
	const AUTO(acceptor, boost::make_shared<Specialized_acceptor>(bind, port, appkey));
	acceptor->activate();
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

boost::shared_ptr<Common::Interserver_connection> Pilot_acceptor::get_session(const Poseidon::Uuid &connection_uuid){
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		CIRCE_LOG_WARNING("Pilot_acceptor has not been initialized.");
		return VAL_INIT;
	}
	return acceptor->get_session(connection_uuid);
}
std::size_t Pilot_acceptor::get_all_sessions(boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > &sessions_ret){
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		CIRCE_LOG_WARNING("Pilot_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->get_all_sessions(sessions_ret);
}
std::size_t Pilot_acceptor::safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		CIRCE_LOG_WARNING("Pilot_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->safe_broadcast_notification(msg);
}
std::size_t Pilot_acceptor::clear(long err_code, const char *err_msg) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		CIRCE_LOG_WARNING("Pilot_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->clear(err_code, err_msg);
}

}
}
