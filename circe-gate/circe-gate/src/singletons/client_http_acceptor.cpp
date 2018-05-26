// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_acceptor.hpp"
#include "../mmain.hpp"
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/singletons/epoll_daemon.hpp>

namespace Circe {
namespace Gate {

namespace {
	class Specialized_acceptor : public Poseidon::Tcp_server_base {
	private:
		mutable Poseidon::Mutex m_mutex;
		boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<Client_http_session> > m_weak_sessions;

	public:
		Specialized_acceptor(const std::string &bind, boost::uint16_t port, const std::string &cert, const std::string &pkey)
			: Poseidon::Tcp_server_base(Poseidon::Ip_port(bind.c_str(), port), cert.c_str(), pkey.c_str())
		{
			//
		}
		~Specialized_acceptor() OVERRIDE {
			clear(Poseidon::Websocket::status_going_away);
		}

	protected:
		boost::shared_ptr<Poseidon::Tcp_session_base> on_client_connect(Poseidon::Move<Poseidon::Unique_file> client) OVERRIDE {
			PROFILE_ME;

			AUTO(session, boost::make_shared<Client_http_session>(STD_MOVE(client)));
			session->set_no_delay();
			{
				const Poseidon::Mutex::Unique_lock lock(m_mutex);
				bool erase_it;
				for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); erase_it ? (it = m_weak_sessions.erase(it)) : ++it){
					erase_it = it->second.expired();
				}
				DEBUG_THROW_ASSERT(m_weak_sessions.emplace(session->get_client_uuid(), session).second);
			}
			return STD_MOVE_IDN(session);
		}

	public:
		boost::shared_ptr<Client_http_session> get_session(const Poseidon::Uuid &client_uuid) const {
			PROFILE_ME;

			const Poseidon::Mutex::Unique_lock lock(m_mutex);
			const AUTO(it, m_weak_sessions.find(client_uuid));
			if(it == m_weak_sessions.end()){
				return VAL_INIT;
			}
			AUTO(http_session, it->second.lock());
			return STD_MOVE_IDN(http_session);
		}
		std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Client_http_session> > &http_sessions_ret){
			PROFILE_ME;

			const Poseidon::Mutex::Unique_lock lock(m_mutex);
			std::size_t count_added = 0;
			http_sessions_ret.reserve(http_sessions_ret.size() + m_weak_sessions.size());
			for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
				AUTO(http_session, it->second.lock());
				if(!http_session){
					continue;
				}
				http_sessions_ret.emplace_back(STD_MOVE(http_session));
				++count_added;
			}
			return count_added;
		}
		boost::shared_ptr<Client_websocket_session> get_websocket_session(const Poseidon::Uuid &client_uuid) const {
			PROFILE_ME;

			const Poseidon::Mutex::Unique_lock lock(m_mutex);
			const AUTO(it, m_weak_sessions.find(client_uuid));
			if(it == m_weak_sessions.end()){
				return VAL_INIT;
			}
			AUTO(http_session, it->second.lock());
			if(!http_session){
				return VAL_INIT;
			}
			AUTO(ws_session, boost::dynamic_pointer_cast<Client_websocket_session>(http_session->get_upgraded_session()));
			return STD_MOVE_IDN(ws_session);
		}
		std::size_t get_all_websocket_sessions(boost::container::vector<boost::shared_ptr<Client_websocket_session> > &ws_sessions_ret){
			PROFILE_ME;

			const Poseidon::Mutex::Unique_lock lock(m_mutex);
			std::size_t count_added = 0;
			ws_sessions_ret.reserve(ws_sessions_ret.size() + m_weak_sessions.size());
			for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
				AUTO(http_session, it->second.lock());
				if(!http_session){
					continue;
				}
				AUTO(ws_session, boost::dynamic_pointer_cast<Client_websocket_session>(http_session->get_upgraded_session()));
				if(!ws_session){
					continue;
				}
				ws_sessions_ret.emplace_back(STD_MOVE(ws_session));
				++count_added;
			}
			return count_added;
		}
		std::size_t clear(Poseidon::Websocket::Status_code status_code, const char *reason = "") NOEXCEPT {
			PROFILE_ME;

			const Poseidon::Mutex::Unique_lock lock(m_mutex);
			std::size_t count_shutdown = 0;
			for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
				AUTO(http_session, it->second.lock());
				if(!http_session){
					continue;
				}
				LOG_CIRCE_DEBUG("Disconnecting client session: remote = ", http_session->get_remote_info());
				http_session->shutdown(status_code, reason);
				++count_shutdown;
			}
			return count_shutdown;
		}
	};

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Specialized_acceptor> g_weak_acceptor;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(bind, get_config<std::string>("client_http_acceptor_bind"));
	const AUTO(port, get_config<boost::uint16_t>("client_http_acceptor_port"));
	const AUTO(cert, get_config<std::string>("client_http_acceptor_certificate"));
	const AUTO(pkey, get_config<std::string>("client_http_acceptor_private_key"));
	const AUTO(acceptor, boost::make_shared<Specialized_acceptor>(bind, port, cert, pkey));
	Poseidon::Epoll_daemon::add_socket(acceptor, false);
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

boost::shared_ptr<Client_http_session> Client_http_acceptor::get_session(const Poseidon::Uuid &client_uuid){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		LOG_CIRCE_WARNING("Client_http_acceptor has not been initialized.");
		return VAL_INIT;
	}
	return acceptor->get_session(client_uuid);
}
std::size_t Client_http_acceptor::get_all_sessions(boost::container::vector<boost::shared_ptr<Client_http_session> > &sessions_ret){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		LOG_CIRCE_WARNING("Client_http_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->get_all_sessions(sessions_ret);
}
boost::shared_ptr<Client_websocket_session> Client_http_acceptor::get_websocket_session(const Poseidon::Uuid &client_uuid){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		LOG_CIRCE_WARNING("Client_http_acceptor has not been initialized.");
		return VAL_INIT;
	}
	return acceptor->get_websocket_session(client_uuid);
}
std::size_t Client_http_acceptor::get_all_websocket_sessions(boost::container::vector<boost::shared_ptr<Client_websocket_session> > &sessions_ret){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		LOG_CIRCE_WARNING("Client_http_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->get_all_websocket_sessions(sessions_ret);
}
std::size_t Client_http_acceptor::clear(Poseidon::Websocket::Status_code status_code, const char *reason) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		LOG_CIRCE_WARNING("Client_http_acceptor has not been initialized.");
		return 0;
	}
	return acceptor->clear(status_code, reason);
}

}
}
