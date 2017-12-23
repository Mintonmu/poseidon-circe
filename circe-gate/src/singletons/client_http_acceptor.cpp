// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_acceptor.hpp"
#include "../client_websocket_session.hpp"
#include "../mmain.hpp"
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/singletons/epoll_daemon.hpp>

namespace Circe {
namespace Gate {

namespace {
	class SpecializedAcceptor : public Poseidon::TcpServerBase {
	private:
		mutable Poseidon::Mutex m_mutex;
		boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<ClientHttpSession> > m_weak_sessions;

	public:
		SpecializedAcceptor(const std::string &bind, unsigned port, const std::string &cert, const std::string &pkey)
			: Poseidon::TcpServerBase(Poseidon::IpPort(bind.c_str(), port), cert.c_str(), pkey.c_str())
		{ }
		~SpecializedAcceptor() OVERRIDE {
			clear();
		}

	protected:
		boost::shared_ptr<Poseidon::TcpSessionBase> on_client_connect(Poseidon::Move<Poseidon::UniqueFile> client) OVERRIDE {
			PROFILE_ME;

			AUTO(session, boost::make_shared<ClientHttpSession>(STD_MOVE(client)));
			session->set_no_delay();
			{
				const Poseidon::Mutex::UniqueLock lock(m_mutex);
				bool erase_it;
				for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); erase_it ? (it = m_weak_sessions.erase(it)) : ++it){
					erase_it = it->second.expired();
				}
				DEBUG_THROW_ASSERT(m_weak_sessions.emplace(session->get_session_uuid(), session).second);
			}
			return STD_MOVE_IDN(session);
		}

	public:
		boost::shared_ptr<ClientHttpSession> get_session(const Poseidon::Uuid &session_uuid) const {
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			const AUTO(it, m_weak_sessions.find(session_uuid));
			if(it == m_weak_sessions.end()){
				return VAL_INIT;
			}
			return it->second.lock();
		}
		void clear() NOEXCEPT {
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			for(AUTO(it, m_weak_sessions.begin()); it != m_weak_sessions.end(); ++it){
				const AUTO(http_session, it->second.lock());
				if(http_session){
					LOG_CIRCE_DEBUG("Disconnecting client session: remote = ", http_session->get_remote_info());
					const AUTO(ws_session, boost::dynamic_pointer_cast<ClientWebSocketSession>(http_session->get_upgraded_session()));
					if(ws_session){
						ws_session->shutdown(Poseidon::WebSocket::ST_GOING_AWAY);
					} else {
						http_session->force_shutdown();
					}
				}
			}
		}
	};

	boost::weak_ptr<SpecializedAcceptor> g_weak_acceptor;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const AUTO(bind, get_config<std::string>("client_http_acceptor_bind", "127.0.0.1"));
	const AUTO(port, get_config<boost::uint16_t>("client_http_acceptor_port", 10810));
	const AUTO(cert, get_config<std::string>("client_http_acceptor_certificate"));
	const AUTO(pkey, get_config<std::string>("client_http_acceptor_private_key"));
	const AUTO(acceptor, boost::make_shared<SpecializedAcceptor>(bind, port, cert, pkey));
	Poseidon::EpollDaemon::add_socket(acceptor, false);
	handles.push(acceptor);
	g_weak_acceptor = acceptor;
}

boost::shared_ptr<ClientHttpSession> ClientHttpAcceptor::get_session(const Poseidon::Uuid &session_uuid){
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return VAL_INIT;
	}
	return acceptor->get_session(session_uuid);
}
void ClientHttpAcceptor::clear(long /*err_code*/, const char */*err_msg*/) NOEXCEPT {
	PROFILE_ME;

	const AUTO(acceptor, g_weak_acceptor.lock());
	if(!acceptor){
		return;
	}
	acceptor->clear();
}

}
}
