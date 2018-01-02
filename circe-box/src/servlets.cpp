// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_box.hpp"
#include "protocol/utilities.hpp"
#include "singletons/websocket_shadow_session_supervisor.hpp"
#include "user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Box::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET_FOR(Protocol::Box::HttpRequest, /*conn*/, req){
	Poseidon::Http::StatusCode resp_status_code = Poseidon::Http::ST_SERVICE_UNAVAILABLE;
	Poseidon::OptionalMap resp_headers;
	Poseidon::StreamBuffer resp_entity;
	UserDefinedFunctions::handle_http_request(resp_status_code, resp_headers, resp_entity,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.auth_token), boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri),
		Protocol::copy_key_values(STD_MOVE(req.params)), Protocol::copy_key_values(STD_MOVE(req.headers)), STD_MOVE(req.entity));

	Protocol::Box::HttpResponse resp;
	resp.status_code = resp_status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(resp_headers));
	resp.entity      = STD_MOVE(resp_entity);
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::WebSocketEstablishmentRequest, conn, req){
	const AUTO(shadow_session, boost::make_shared<WebSocketShadowSession>(conn->get_connection_uuid(), Poseidon::Uuid(req.gate_uuid), Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.auth_token)));
	UserDefinedFunctions::handle_websocket_establishment(shadow_session, STD_MOVE(req.decoded_uri), Protocol::copy_key_values(STD_MOVE(req.params)));
	WebSocketShadowSessionSupervisor::insert_session(shadow_session);

	Protocol::Box::WebSocketEstablishmentResponse resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::WebSocketClosureNotification, /*conn*/, ntfy){
	const AUTO(shadow_session, WebSocketShadowSessionSupervisor::detach_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(shadow_session){
		shadow_session->mark_shutdown();
		try {
			UserDefinedFunctions::handle_websocket_closure(shadow_session, boost::numeric_cast<Poseidon::WebSocket::StatusCode>(ntfy.status_code), ntfy.reason.c_str());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		}
	}

	return 0;
}

DEFINE_SERVLET_FOR(Protocol::Box::WebSocketPackedMessageRequest, /*conn*/, req){
	const AUTO(shadow_session, WebSocketShadowSessionSupervisor::get_session(Poseidon::Uuid(req.client_uuid)));
	if(shadow_session){
		try {
			while(!req.messages.empty()){
				UserDefinedFunctions::handle_websocket_message(shadow_session, boost::numeric_cast<Poseidon::WebSocket::OpCode>(req.messages.front().opcode), STD_MOVE(req.messages.front().payload));
				req.messages.pop_front();
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			shadow_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}

	Protocol::Box::WebSocketPackedMessageResponse resp;
	return resp;
}

}
}
