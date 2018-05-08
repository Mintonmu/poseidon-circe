// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "websocket_shadow_session_supervisor.hpp"
#include "box_acceptor.hpp"
#include "protocol/messages_foyer.hpp"
#include "user_defined_functions.hpp"
#include "../mmain.hpp"
#include <poseidon/multi_index_map.hpp>
#include <poseidon/singletons/timer_daemon.hpp>

namespace Circe {
namespace Box {

namespace {
	struct Session_element {
		// Invairants.
		boost::shared_ptr<Web_socket_shadow_session> session;
		// Indices.
		Poseidon::Uuid client_uuid;
		std::pair<Poseidon::Uuid, Poseidon::Uuid> foyer_gate_uuid_pair;
	};
	MULTI_INDEX_MAP(Session_container, Session_element,
		UNIQUE_MEMBER_INDEX(client_uuid)
		MULTI_MEMBER_INDEX(foyer_gate_uuid_pair)
	);

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Session_container> g_weak_session_container;

	void ping_timer_proc(){
		PROFILE_ME;

		const AUTO(session_container, g_weak_session_container.lock());
		if(!session_container){
			return;
		}

		// Collect gate servers.
		boost::container::vector<std::pair<Poseidon::Uuid, Poseidon::Uuid> > foyer_gate_uuids_expired;
		{
			const Poseidon::Mutex::Unique_lock lock(g_mutex);
			for(AUTO(it, session_container->begin<1>()); it != session_container->end<1>(); it = session_container->upper_bound<1>(it->foyer_gate_uuid_pair)){
				foyer_gate_uuids_expired.emplace_back(it->foyer_gate_uuid_pair);
			}
		}
		// Check them. If a server is alive, it should be erased from the session_container.
		bool erase_it;
		for(AUTO(it, foyer_gate_uuids_expired.begin()); it != foyer_gate_uuids_expired.end(); erase_it ? (it = foyer_gate_uuids_expired.erase(it)) : ++it){
			const AUTO(foyer_uuid, it->first);
			const AUTO(gate_uuid, it->second);
			LOG_CIRCE_DEBUG("Checking gate: foyer_uuid = ", foyer_uuid, ", gate_uuid = ", gate_uuid);
			try {
				const AUTO(foyer_conn, Box_acceptor::get_session(foyer_uuid));
				DEBUG_THROW_UNLESS(foyer_conn, Poseidon::Exception, Poseidon::Rcnts::view("Connection to foyer server was lost"));
				{
					Protocol::Foyer::Check_gate_request foyer_req;
					foyer_req.gate_uuid = gate_uuid;
					LOG_CIRCE_TRACE("Sending request: ", foyer_req);
					Protocol::Foyer::Check_gate_response foyer_resp;
					Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
					LOG_CIRCE_TRACE("Received response: ", foyer_resp);
				}
				LOG_CIRCE_DEBUG("Gate is alive: foyer_uuid = ", foyer_uuid, ", gate_uuid = ", gate_uuid);
				erase_it = true;
			} catch(std::exception &e){
				LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
				erase_it = false;
			}
		}
		// Remove sessions whose corresponding gate servers no longer exist.
		boost::container::vector<boost::shared_ptr<Web_socket_shadow_session> > sessions_detached;
		for(AUTO(it, foyer_gate_uuids_expired.begin()); it != foyer_gate_uuids_expired.end(); ++it){
			sessions_detached.clear();
			{
				const Poseidon::Mutex::Unique_lock lock(g_mutex);
				const AUTO(range, session_container->equal_range<1>(foyer_gate_uuids_expired.back()));
				sessions_detached.reserve(static_cast<std::size_t>(std::distance(range.first, range.second)));
				for(AUTO(sit, range.first); sit != range.second; ++sit){
					sessions_detached.push_back(sit->session);
				}
				session_container->erase<1>(range.first, range.second);
			}
			for(AUTO(sit, sessions_detached.begin()); sit != sessions_detached.end(); ++sit){
				const AUTO_REF(session, *sit);
				LOG_CIRCE_DEBUG("Disconnecting Web_socket_shadow_session: client_ip = ", session->get_client_ip());
				session->mark_shutdown();
				try {
					User_defined_functions::handle_websocket_closure(session, Poseidon::Web_socket::status_going_away, "Connection to gate server was lost");
				} catch(std::exception &e){
					LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
				}
			}
		}
	}
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, boost::make_shared<Session_container>());
	handles.push(session_container);
	g_weak_session_container = session_container;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const AUTO(ping_timer_interval, get_config<boost::uint64_t>("websocket_shadow_session_ping_timer_interval", 60000));
	const AUTO(timer, Poseidon::Timer_daemon::register_timer(0, ping_timer_interval, boost::bind(&ping_timer_proc)));
	handles.push(timer);
}

boost::shared_ptr<Web_socket_shadow_session> Web_socket_shadow_session_supervisor::get_session(const Poseidon::Uuid &client_uuid){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, g_weak_session_container.lock());
	if(!session_container){
		LOG_CIRCE_WARNING("Web_socket_shadow_session_supervisor has not been initialized.");
		return VAL_INIT;
	}
	const AUTO(it, session_container->find<0>(client_uuid));
	if(it == session_container->end<0>()){
		return VAL_INIT;
	}
	AUTO(session, it->session);
	return STD_MOVE(session);
}
std::size_t Web_socket_shadow_session_supervisor::get_all_sessions(boost::container::vector<boost::shared_ptr<Web_socket_shadow_session> > &sessions_ret){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, g_weak_session_container.lock());
	if(!session_container){
		LOG_CIRCE_WARNING("Web_socket_shadow_session_supervisor has not been initialized.");
		return 0;
	}
	std::size_t count_added = 0;
	sessions_ret.reserve(sessions_ret.size() + session_container->size());
	for(AUTO(it, session_container->begin()); it != session_container->end(); ++it){
		AUTO(session, it->session);
		sessions_ret.emplace_back(STD_MOVE(session));
		++count_added;
	}
	return count_added;
}
void Web_socket_shadow_session_supervisor::attach_session(const boost::shared_ptr<Web_socket_shadow_session> &session){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, g_weak_session_container.lock());
	if(!session_container){
		LOG_CIRCE_WARNING("Web_socket_shadow_session_supervisor has not been initialized.");
		DEBUG_THROW(Poseidon::Exception, Poseidon::Rcnts::view("Web_socket_shadow_session_supervisor has not been initialized"));
	}
	Session_element elem = { session, session->get_client_uuid(), std::make_pair(session->get_foyer_uuid(), session->get_gate_uuid()) };
	const AUTO(pair, session_container->insert(STD_MOVE(elem)));
	DEBUG_THROW_UNLESS(pair.second, Poseidon::Exception, Poseidon::Rcnts::view("Duplicate Web_socket_shadow_session UUID"));
}
boost::shared_ptr<Web_socket_shadow_session> Web_socket_shadow_session_supervisor::detach_session(const Poseidon::Uuid &client_uuid) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, g_weak_session_container.lock());
	if(!session_container){
		LOG_CIRCE_WARNING("Web_socket_shadow_session_supervisor has not been initialized.");
		return VAL_INIT;
	}
	const AUTO(it, session_container->find<0>(client_uuid));
	if(it == session_container->end<0>()){
		return VAL_INIT;
	}
	AUTO(session, it->session);
	session_container->erase<0>(it);
	return STD_MOVE(session);
}
std::size_t Web_socket_shadow_session_supervisor::clear(Poseidon::Web_socket::Status_code status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(session_container, g_weak_session_container.lock());
	if(!session_container){
		LOG_CIRCE_WARNING("Web_socket_shadow_session_supervisor has not been initialized.");
		return 0;
	}
	std::size_t count_shutdown = 0;
	for(AUTO(it, session_container->begin()); it != session_container->end(); it = session_container->erase(it)){
		AUTO(session, it->session);
		LOG_CIRCE_DEBUG("Disconnecting Web_socket_shadow_session: client_ip = ", session->get_client_ip());
		session->shutdown(status_code, reason);
		++count_shutdown;
	}
	return count_shutdown;
}

}
}
