// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_SINGLETONS_CLIENT_HTTP_ACCEPTOR_HPP_
#define CIRCE_GATE_SINGLETONS_CLIENT_HTTP_ACCEPTOR_HPP_

#include "../client_http_session.hpp"
#include "../client_websocket_session.hpp"

namespace Circe {
namespace Gate {

class Client_http_acceptor {
private:
	Client_http_acceptor();

public:
	static boost::shared_ptr<Client_http_session> get_session(const Poseidon::Uuid &client_uuid);
	static std::size_t get_all_sessions(boost::container::vector<boost::shared_ptr<Client_http_session> > &sessions_ret);
	static boost::shared_ptr<Client_websocket_session> get_websocket_session(const Poseidon::Uuid &client_uuid);
	static std::size_t get_all_websocket_sessions(boost::container::vector<boost::shared_ptr<Client_websocket_session> > &sessions_ret);
	static std::size_t clear(Poseidon::Websocket::Status_code status_code, const char *reason = "") NOEXCEPT;
};

}
}

#endif
