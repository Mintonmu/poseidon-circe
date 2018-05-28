// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_auth.hpp"
#include "singletons/user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Auth::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Auth {

DEFINE_SERVLET_FOR(Protocol::Auth::Http_authentication_request, /*connection*/, req){
	Poseidon::Http::Status_code resp_status_code = Poseidon::Http::status_service_unavailable;
	Poseidon::Option_map resp_headers, params, headers;
	for(AUTO(it, req.params.begin()); it != req.params.end(); ++it){
		params.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	for(AUTO(it, req.headers.begin()); it != req.headers.end(); ++it){
		headers.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	std::string auth_token = User_defined_functions::check_http_authentication(resp_status_code, resp_headers, Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip),
		boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri), STD_MOVE(params), STD_MOVE(headers));

	Protocol::Auth::Http_authentication_response resp;
	resp.auth_token  = STD_MOVE(auth_token);
	resp.status_code = resp_status_code;
	for(AUTO(it, resp_headers.begin()); it != resp_headers.end(); ++it){
		Protocol::Common_key_value option;
		option.key   = it->first.get();
		option.value = STD_MOVE(it->second);
		resp.headers.push_back(STD_MOVE(option));
	}
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Auth::Websocket_authentication_request, /*connection*/, req){
	boost::container::deque<std::pair<Poseidon::Websocket::Opcode, Poseidon::Stream_buffer> > resp_messages;
	Poseidon::Websocket::Status_code resp_status_code = Poseidon::Websocket::status_internal_error;
	std::string resp_reason;
	Poseidon::Option_map params;
	for(AUTO(it, req.params.begin()); it != req.params.end(); ++it){
		params.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	std::string auth_token = User_defined_functions::check_websocket_authentication(resp_messages, resp_status_code, resp_reason, Poseidon::Uuid(req.client_uuid),
		STD_MOVE(req.client_ip), STD_MOVE(req.decoded_uri), STD_MOVE_IDN(params));

	Protocol::Auth::Websocket_authentication_response resp;
	for(AUTO(it, resp_messages.begin()); it != resp_messages.end(); ++it){
		Protocol::Common_websocket_frame frame;
		frame.opcode  = boost::numeric_cast<unsigned>(it->first);
		frame.payload = STD_MOVE(it->second);
		resp.messages.push_back(STD_MOVE(frame));
	}
	resp.auth_token  = STD_MOVE(auth_token);
	resp.status_code = resp_status_code;
	resp.reason      = STD_MOVE(resp_reason);
	return resp;
}

}
}
