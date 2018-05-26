// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/client_http_acceptor.hpp"
#include "singletons/ip_ban_list.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Gate::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Gate {

DEFINE_SERVLET_FOR(Protocol::Gate::Websocket_kill_notification, /*connection*/, ntfy){
	const AUTO(ws_session, Client_http_acceptor::get_websocket_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(ws_session){
		LOG_CIRCE_INFO("Killing client WebSocket session: remote = ", ws_session->get_remote_info(), ", ntfy = ", ntfy);
		ws_session->shutdown(boost::numeric_cast<Poseidon::Websocket::Status_code>(ntfy.status_code), ntfy.reason.c_str());
	}

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Gate::Websocket_packed_message_request, /*connection*/, req){
	const AUTO(ws_session, Client_http_acceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		try {
			for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
				ws_session->send(boost::numeric_cast<Poseidon::Websocket::Op_code>(it->opcode), STD_MOVE(it->payload));
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::Websocket::status_internal_error, e.what());
		}
	}

	Protocol::Gate::Websocket_packed_message_response resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Gate::Websocket_packed_broadcast_notification, /*connection*/, req){
	for(AUTO(qcit, req.clients.begin()); qcit != req.clients.end(); ++qcit){
		const AUTO(ws_session, Client_http_acceptor::get_websocket_session(Poseidon::Uuid(qcit->client_uuid)));
		if(ws_session){
			try {
				for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
					ws_session->send(boost::numeric_cast<Poseidon::Websocket::Op_code>(it->opcode), it->payload);
				}
			} catch(std::exception &e){
				LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
				ws_session->shutdown(Poseidon::Websocket::status_internal_error, e.what());
			}
		}
	}

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Gate::Unban_ip_request, /*connection*/, req){
	const bool found = Ip_ban_list::remove_ban(req.client_ip.c_str());

	Protocol::Gate::Unban_ip_response resp;
	resp.found = found;
	return resp;
}

}
}
