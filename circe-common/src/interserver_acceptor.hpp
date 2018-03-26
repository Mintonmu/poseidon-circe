// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_
#define CIRCE_COMMON_INTERSERVER_ACCEPTOR_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include "interserver_servlet_callback.hpp"

namespace Circe {
namespace Common {

class Interserver_connection;
class Interserver_response;

class Interserver_acceptor : public virtual Poseidon::Virtual_shared_from_this {
private:
	class Interserver_session;
	class Interserver_server;

private:
	const std::string m_bind;
	const boost::uint16_t m_port;
	const std::string m_application_key;

	mutable Poseidon::Mutex m_mutex;
	boost::shared_ptr<Interserver_server> m_server;
	boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<Interserver_session> > m_weak_sessions;

public:
	Interserver_acceptor(std::string bind, boost::uint16_t port, std::string application_key);
	~Interserver_acceptor() OVERRIDE;

protected:
	virtual boost::shared_ptr<const Interserver_servlet_callback> sync_get_servlet(boost::uint16_t message_id) const = 0;

public:
	void activate();

	boost::shared_ptr<Interserver_connection> get_session(const Poseidon::Uuid &connection_uuid) const;
	std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Interserver_connection> > &sessions_ret) const;
	std::size_t safe_broadcast_notification(const Poseidon::Cbpp::Message_base &msg) const NOEXCEPT;
	std::size_t clear(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
