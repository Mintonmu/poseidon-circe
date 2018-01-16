// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "websocket_shadow_session_supervisor.hpp"
#include "box_acceptor.hpp"
#include "protocol/messages_foyer.hpp"
#include "../user_defined_functions.hpp"
#include "../mmain.hpp"
#include <poseidon/multi_index_map.hpp>
#include <poseidon/singletons/timer_daemon.hpp>

namespace Circe {
namespace Box {

namespace {
	class SessionContainer : NONCOPYABLE {
	private:
		struct Element {
			// Invairants.
			boost::shared_ptr<WebSocketShadowSession> session;
			// Indices.
			Poseidon::Uuid client_uuid;
			std::pair<Poseidon::Uuid, Poseidon::Uuid> foyer_gate_uuid_pair;
		};
		MULTI_INDEX_MAP(Map, Element,
			UNIQUE_MEMBER_INDEX(client_uuid)
			MULTI_MEMBER_INDEX(foyer_gate_uuid_pair)
		);

	private:
		mutable Poseidon::Mutex m_mutex;
		Map m_map;

	public:
		SessionContainer()
			: m_map()
		{ }
		~SessionContainer(){
			clear(Poseidon::WebSocket::ST_GOING_AWAY);
		}

	public:
		boost::shared_ptr<WebSocketShadowSession> get_session(const Poseidon::Uuid &client_uuid){
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			const AUTO(it, m_map.find<0>(client_uuid));
			if(it == m_map.end<0>()){
				return VAL_INIT;
			}
			AUTO(session, it->session);
			return STD_MOVE(session);
		}
		std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<WebSocketShadowSession> > &sessions_ret){
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			std::size_t count_added = 0;
			sessions_ret.reserve(sessions_ret.size() + m_map.size());
			for(AUTO(it, m_map.begin()); it != m_map.end(); ++it){
				AUTO(session, it->session);
				sessions_ret.emplace_back(STD_MOVE(session));
				++count_added;
			}
			return count_added;
		}
		void attach_session(const boost::shared_ptr<WebSocketShadowSession> &session){
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			Element elem = { session, session->get_client_uuid(), std::make_pair(session->get_foyer_uuid(), session->get_gate_uuid()) };
			const AUTO(pair, m_map.insert(STD_MOVE(elem)));
			DEBUG_THROW_UNLESS(pair.second, Poseidon::Exception, Poseidon::sslit("Duplicate WebSocketShadowSession UUID"));
		}
		boost::shared_ptr<WebSocketShadowSession> detach_session(const Poseidon::Uuid &client_uuid) NOEXCEPT {
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			const AUTO(it, m_map.find<0>(client_uuid));
			if(it == m_map.end<0>()){
				return VAL_INIT;
			}
			AUTO(session, it->session);
			m_map.erase<0>(it);
			return STD_MOVE(session);
		}
		std::size_t clear(Poseidon::WebSocket::StatusCode status_code, const char *reason = "") NOEXCEPT {
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			std::size_t count_shutdown = 0;
			for(AUTO(it, m_map.begin()); it != m_map.end(); it = m_map.erase(it)){
				AUTO(session, it->session);
				LOG_CIRCE_DEBUG("Disconnecting WebSocketShadowSession: client_ip = ", session->get_client_ip());
				session->shutdown(status_code, reason);
				++count_shutdown;
			}
			return count_shutdown;
		}

		void sync_check_gates(){
			PROFILE_ME;

			// Collect gate servers.
			boost::container::vector<std::pair<Poseidon::Uuid, Poseidon::Uuid> > foyer_gate_uuids_expired;
			{
				const Poseidon::Mutex::UniqueLock lock(m_mutex);
				for(AUTO(it, m_map.begin<1>()); it != m_map.end<1>(); it = m_map.upper_bound<1>(it->foyer_gate_uuid_pair)){
					foyer_gate_uuids_expired.emplace_back(it->foyer_gate_uuid_pair);
				}
			}
			// Check them, keeping servers whose connections are lost.
			bool erase_it;
			for(AUTO(it, foyer_gate_uuids_expired.begin()); it != foyer_gate_uuids_expired.end(); erase_it ? (it = foyer_gate_uuids_expired.erase(it)) : ++it){
				const AUTO(foyer_uuid, it->first);
				const AUTO(gate_uuid, it->second);
				LOG_CIRCE_DEBUG("Checking gate: foyer_uuid = ", foyer_uuid, ", gate_uuid = ", gate_uuid);

				try {
					const AUTO(foyer_conn, BoxAcceptor::get_session(foyer_uuid));
					DEBUG_THROW_UNLESS(foyer_conn, Poseidon::Exception, Poseidon::sslit("Connection to foyer server was lost"));

					Protocol::Foyer::CheckGateRequest foyer_req;
					foyer_req.gate_uuid = gate_uuid;
					LOG_CIRCE_TRACE("Sending request: ", foyer_req);
					Protocol::Foyer::CheckGateResponse foyer_resp;
					Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
					LOG_CIRCE_TRACE("Received response: ", foyer_resp);

					LOG_CIRCE_DEBUG("Gate is alive: foyer_uuid = ", foyer_uuid, ", gate_uuid = ", gate_uuid);
					erase_it = true;
				} catch(std::exception &e){
					LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
					erase_it = false;
				}
			}
			// Remove sessions whose corresponding gate servers no longer exist.
			boost::container::vector<boost::shared_ptr<WebSocketShadowSession> > sessions_to_erase;
			while(!foyer_gate_uuids_expired.empty()){
				sessions_to_erase.clear();
				{
					const Poseidon::Mutex::UniqueLock lock(m_mutex);
					const AUTO(range, m_map.equal_range<1>(foyer_gate_uuids_expired.back()));
					sessions_to_erase.reserve(static_cast<std::size_t>(std::distance(range.first, range.second)));
					for(AUTO(it, range.first); it != range.second; it = m_map.erase<1>(it)){
						sessions_to_erase.emplace_back(it->session);
					}
				}
				while(!sessions_to_erase.empty()){
					const AUTO(session, STD_MOVE_IDN(sessions_to_erase.back()));
					LOG_CIRCE_DEBUG("Disconnecting WebSocketShadowSession: client_ip = ", session->get_client_ip());
					session->mark_shutdown();
					try {
						UserDefinedFunctions::handle_websocket_closure(session, Poseidon::WebSocket::ST_GOING_AWAY, "Connection to gate server was lost");
					} catch(std::exception &e){
						LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
					}
					sessions_to_erase.pop_back();
				}
				foyer_gate_uuids_expired.pop_back();
			}
		}
	};

	boost::weak_ptr<SessionContainer> g_container;

	void ping_timer_proc(){
		PROFILE_ME;

		const AUTO(container, g_container.lock());
		if(!container){
			return;
		}

		container->sync_check_gates();
	}
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const AUTO(container, boost::make_shared<SessionContainer>());
	handles.push(container);
	g_container = container;

	const AUTO(ping_interval, get_config<boost::uint64_t>("websocket_shadow_session_ping_interval", 60000));
	const AUTO(timer, Poseidon::TimerDaemon::register_timer(0, ping_interval, boost::bind(&ping_timer_proc)));
	handles.push(timer);
}

boost::shared_ptr<WebSocketShadowSession> WebSocketShadowSessionSupervisor::get_session(const Poseidon::Uuid &client_uuid){
	PROFILE_ME;

	const AUTO(container, g_container.lock());
	if(!container){
		LOG_CIRCE_WARNING("WebSocketShadowSessionSupervisor has not been initialized.");
		return VAL_INIT;
	}
	return container->get_session(client_uuid);
}
std::size_t WebSocketShadowSessionSupervisor::get_all_sessions(boost::container::vector<boost::shared_ptr<WebSocketShadowSession> > &sessions_ret){
	PROFILE_ME;

	const AUTO(container, g_container.lock());
	if(!container){
		LOG_CIRCE_WARNING("WebSocketShadowSessionSupervisor has not been initialized.");
		return 0;
	}
	return container->get_all_sessions(sessions_ret);
}
void WebSocketShadowSessionSupervisor::attach_session(const boost::shared_ptr<WebSocketShadowSession> &session){
	PROFILE_ME;

	const AUTO(container, g_container.lock());
	if(!container){
		LOG_CIRCE_WARNING("WebSocketShadowSessionSupervisor has not been initialized.");
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("WebSocketShadowSessionSupervisor has not been initialized"));
	}
	return container->attach_session(session);
}
boost::shared_ptr<WebSocketShadowSession> WebSocketShadowSessionSupervisor::detach_session(const Poseidon::Uuid &client_uuid) NOEXCEPT {
	PROFILE_ME;

	const AUTO(container, g_container.lock());
	if(!container){
		LOG_CIRCE_WARNING("WebSocketShadowSessionSupervisor has not been initialized.");
		return VAL_INIT;
	}
	return container->detach_session(client_uuid);
}
std::size_t WebSocketShadowSessionSupervisor::clear(Poseidon::WebSocket::StatusCode status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const AUTO(container, g_container.lock());
	if(!container){
		LOG_CIRCE_WARNING("WebSocketShadowSessionSupervisor has not been initialized.");
		return VAL_INIT;
	}
	return container->clear(status_code, reason);
}

}
}
