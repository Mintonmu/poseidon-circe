// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_acceptor.hpp"
#include "../mmain.hpp"
#include <poseidon/multi_index_map.hpp>
#include <poseidon/tcp_server_base.hpp>
#include <poseidon/singletons/epoll_daemon.hpp>

namespace Circe {
namespace Gate {

namespace {
	struct SessionElement {
		// Invariants.
		boost::weak_ptr<ClientHttpSession> weak_session;
		// Indices.
		const volatile ClientHttpSession *ptr;
		Poseidon::Uuid session_uuid;

		explicit SessionElement(const boost::shared_ptr<ClientHttpSession> &session)
			: weak_session(session)
			, ptr(session.get()), session_uuid(session->get_session_uuid())
		{ }
	};
	MULTI_INDEX_MAP(SessionMap, SessionElement,
		UNIQUE_MEMBER_INDEX(ptr)
		UNIQUE_MEMBER_INDEX(session_uuid)
	)
	Poseidon::Mutex g_session_map_mutex;
	SessionMap g_session_map;

	class ClientTcpServer : public Poseidon::TcpServerBase {
	public:
		ClientTcpServer(const std::string &bind, unsigned port, const std::string &cert, const std::string &pkey)
			: Poseidon::TcpServerBase(Poseidon::IpPort(bind.c_str(), port), cert.c_str(), pkey.c_str())
		{ }

	protected:
		boost::shared_ptr<Poseidon::TcpSessionBase> on_client_connect(Poseidon::Move<Poseidon::UniqueFile> client) OVERRIDE {
			PROFILE_ME;

			const AUTO(session, boost::make_shared<ClientHttpSession>(STD_MOVE(client)));
			session->set_no_delay();
			return session;
		}
	};

	MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
		const AUTO(bind, get_config<std::string>("client_http_acceptor_bind", "127.0.0.1"));
		const AUTO(port, get_config<unsigned>("client_http_acceptor_port", 10810));
		const AUTO(cert, get_config<std::string>("client_http_acceptor_certificate"));
		const AUTO(pkey, get_config<std::string>("client_http_acceptor_private_key"));
		const AUTO(server, boost::make_shared<ClientTcpServer>(bind, port, cert, pkey));
		handles.push(server);
		Poseidon::EpollDaemon::add_socket(server, false);
	}
}

boost::shared_ptr<ClientHttpSession> ClientHttpAcceptor::get_session(const Poseidon::Uuid &session_uuid){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_session_map_mutex);
	const AUTO(it, g_session_map.find<1>(session_uuid));
	if(it == g_session_map.end<1>()){
		return VAL_INIT;
	}
	return it->weak_session.lock();
}
void ClientHttpAcceptor::insert_session(const boost::shared_ptr<ClientHttpSession> &session){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_session_map_mutex);
	const AUTO(pair, g_session_map.insert(SessionElement(session)));
	DEBUG_THROW_UNLESS(pair.second, Poseidon::Exception, Poseidon::sslit("ClientHttpSession is already in map"));
}
bool ClientHttpAcceptor::remove_session(const volatile ClientHttpSession *ptr) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_session_map_mutex);
	return g_session_map.erase<0>(ptr) != 0;
}

}
}
