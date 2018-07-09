// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/websocket_shadow_session_supervisor.hpp"
#include "singletons/user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Box::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET_FOR(Protocol::Box::Http_request, /*connection*/, req){
	Poseidon::Http::Status_code resp_status_code;
	Poseidon::Option_map resp_headers, params, headers;
	Poseidon::Stream_buffer resp_entity;
	for(AUTO(it, req.params.begin()); it != req.params.end(); ++it){
		params.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	for(AUTO(it, req.headers.begin()); it != req.headers.end(); ++it){
		headers.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	resp_status_code = User_defined_functions::handle_http_request(resp_headers, resp_entity, Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip),
		STD_MOVE(req.auth_token), boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri), STD_MOVE(params), STD_MOVE(headers), STD_MOVE(req.entity));

	Protocol::Box::Http_response resp;
	resp.status_code = resp_status_code;
	for(AUTO(it, resp_headers.begin());  it != resp_headers.end(); ++it){
		Protocol::Common_key_value option;
		option.key   = it->first;
		option.value = STD_MOVE(it->second);
		resp.headers.push_back(STD_MOVE(option));
	}
	resp.entity = STD_MOVE(resp_entity);
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::Websocket_establishment_request, connection, req){
	const AUTO(shadow_session, boost::make_shared<Websocket_shadow_session>(connection->get_connection_uuid(),
		Poseidon::Uuid(req.gate_uuid), Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.auth_token)));
	Poseidon::Option_map params;
	for(AUTO(it, req.params.begin()); it != req.params.end(); ++it){
		params.append(Poseidon::Rcnts(it->key), STD_MOVE(it->value));
	}
	User_defined_functions::handle_websocket_establishment(shadow_session, STD_MOVE(req.decoded_uri), STD_MOVE(params));
	Websocket_shadow_session_supervisor::attach_session(shadow_session);

	Protocol::Box::Websocket_establishment_response resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::Websocket_closure_notification, /*connection*/, ntfy){
	const AUTO(shadow_session, Websocket_shadow_session_supervisor::detach_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(shadow_session){
		shadow_session->mark_shutdown();
		try {
			User_defined_functions::handle_websocket_closure(shadow_session, boost::numeric_cast<Poseidon::Websocket::Status_code>(ntfy.status_code), ntfy.reason.c_str());
		} catch(std::exception &e){
			CIRCE_LOG_ERROR("std::exception thrown: what = ", e.what());
		}
	}

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Box::Websocket_packed_message_request, /*connection*/, req){
	const AUTO(shadow_session, Websocket_shadow_session_supervisor::get_session(Poseidon::Uuid(req.client_uuid)));
	if(shadow_session){
		try {
			for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
				User_defined_functions::handle_websocket_message(shadow_session, boost::numeric_cast<Poseidon::Websocket::Opcode>(it->opcode), STD_MOVE(it->payload));
			}
		} catch(std::exception &e){
			CIRCE_LOG_ERROR("std::exception thrown: what = ", e.what());
			shadow_session->shutdown(Poseidon::Websocket::status_internal_error, e.what());
		}
	}

	Protocol::Box::Websocket_packed_message_response resp;
	return resp;
}

}
}
