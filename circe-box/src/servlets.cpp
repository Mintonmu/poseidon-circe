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
#include "singletons/user_defined_functions.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Box::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET_FOR(Protocol::Box::Http_request, /*connection*/, req){
	Poseidon::Http::Status_code resp_status_code = Poseidon::Http::status_service_unavailable;
	Poseidon::Optional_map resp_headers;
	Poseidon::Stream_buffer resp_entity;
	User_defined_functions::handle_http_request(resp_status_code, resp_headers, resp_entity,
		Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.auth_token), boost::numeric_cast<Poseidon::Http::Verb>(req.verb), STD_MOVE(req.decoded_uri),
		Protocol::copy_key_values(STD_MOVE_IDN(req.params)), Protocol::copy_key_values(STD_MOVE_IDN(req.headers)), STD_MOVE(req.entity));

	Protocol::Box::Http_response resp;
	resp.status_code = resp_status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(resp_headers));
	resp.entity      = STD_MOVE(resp_entity);
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::Web_socket_establishment_request, connection, req){
	const AUTO(shadow_session, boost::make_shared<Web_socket_shadow_session>(connection->get_connection_uuid(), Poseidon::Uuid(req.gate_uuid), Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip), STD_MOVE(req.auth_token)));
	User_defined_functions::handle_websocket_establishment(shadow_session, STD_MOVE(req.decoded_uri), Protocol::copy_key_values(STD_MOVE_IDN(req.params)));
	Web_socket_shadow_session_supervisor::attach_session(shadow_session);

	Protocol::Box::Web_socket_establishment_response resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Box::Web_socket_closure_notification, /*connection*/, ntfy){
	const AUTO(shadow_session, Web_socket_shadow_session_supervisor::detach_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(shadow_session){
		shadow_session->mark_shutdown();
		try {
			User_defined_functions::handle_websocket_closure(shadow_session, boost::numeric_cast<Poseidon::Web_socket::Status_code>(ntfy.status_code), ntfy.reason.c_str());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		}
	}

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Box::Web_socket_packed_message_request, /*connection*/, req){
	const AUTO(shadow_session, Web_socket_shadow_session_supervisor::get_session(Poseidon::Uuid(req.client_uuid)));
	if(shadow_session){
		try {
			for(AUTO(qmit, req.messages.begin()); qmit != req.messages.end(); ++qmit){
				User_defined_functions::handle_websocket_message(shadow_session, boost::numeric_cast<Poseidon::Web_socket::Op_code>(qmit->opcode), STD_MOVE(qmit->payload));
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			shadow_session->shutdown(Poseidon::Web_socket::status_internal_error, e.what());
		}
	}

	Protocol::Box::Web_socket_packed_message_response resp;
	return resp;
}

}
}
