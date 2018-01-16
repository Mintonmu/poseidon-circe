// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_auth.hpp"
#include "protocol/utilities.hpp"
#include "user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Auth::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET_FOR(Protocol::Auth::HttpAuthenticationRequest, /*conn*/, req){
	Poseidon::Http::StatusCode resp_status_code = Poseidon::Http::ST_SERVICE_UNAVAILABLE;
	Poseidon::OptionalMap resp_headers;
	std::string auth_token = UserDefinedFunctions::check_http_authentication(resp_status_code, resp_headers,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri),
		Protocol::copy_key_values(STD_MOVE(req.params)), Protocol::copy_key_values(STD_MOVE(req.headers)));

	Protocol::Auth::HttpAuthenticationResponse resp;
	resp.auth_token  = STD_MOVE(auth_token);
	resp.status_code = resp_status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(resp_headers));
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Auth::WebSocketAuthenticationRequest, /*conn*/, req){
	boost::container::deque<std::pair<Poseidon::WebSocket::OpCode, Poseidon::StreamBuffer> > resp_messages;
	Poseidon::WebSocket::StatusCode resp_status_code = Poseidon::WebSocket::ST_INTERNAL_ERROR;
	std::string resp_reason;
	std::string auth_token = UserDefinedFunctions::check_websocket_authentication(resp_messages, resp_status_code, resp_reason,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.decoded_uri), Protocol::copy_key_values(STD_MOVE(req.params)));

	Protocol::Auth::WebSocketAuthenticationResponse resp;
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
