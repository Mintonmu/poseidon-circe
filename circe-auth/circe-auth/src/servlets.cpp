// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_auth.hpp"
#include "protocol/utilities.hpp"
#include "singletons/user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Auth::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET_FOR(Protocol::Auth::Http_authentication_request, /*connection*/, req){
	Poseidon::Http::Status_code resp_status_code = Poseidon::Http::status_service_unavailable;
	Poseidon::Optional_map resp_headers;
	std::string auth_token = User_defined_functions::check_http_authentication(resp_status_code, resp_headers,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri),
		Protocol::copy_key_values(STD_MOVE_IDN(req.params)), Protocol::copy_key_values(STD_MOVE_IDN(req.headers)));

	Protocol::Auth::Http_authentication_response resp;
	resp.auth_token  = STD_MOVE(auth_token);
	resp.status_code = resp_status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(resp_headers));
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Auth::Web_socket_authentication_request, /*connection*/, req){
	boost::container::deque<std::pair<Poseidon::Web_socket::Op_code, Poseidon::Stream_buffer> > resp_messages;
	Poseidon::Web_socket::Status_code resp_status_code = Poseidon::Web_socket::status_internal_error;
	std::string resp_reason;
	std::string auth_token = User_defined_functions::check_websocket_authentication(resp_messages, resp_status_code, resp_reason,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.decoded_uri), Protocol::copy_key_values(STD_MOVE_IDN(req.params)));

	Protocol::Auth::Web_socket_authentication_response resp;
	for(AUTO(qmit, resp_messages.begin()); qmit != resp_messages.end(); ++qmit){
		const AUTO(rmit, Protocol::emplace_at_end(resp.messages));
		rmit->opcode  = boost::numeric_cast<unsigned>(qmit->first);
		rmit->payload = STD_MOVE(qmit->second);
	}
	resp.auth_token  = STD_MOVE(auth_token);
	resp.status_code = resp_status_code;
	resp.reason      = STD_MOVE(resp_reason);
	return resp;
}

}
}
