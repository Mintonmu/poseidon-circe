// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/websocket_shadow_session_supervisor.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Box::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::HttpRequest req){
	LOG_CIRCE_FATAL("TODO: CLIENT HTTP REQUEST: ", req);

	Protocol::Box::HttpResponse resp;
	resp.status_code = 200;
	resp.headers.resize(2);
	resp.headers.at(0).key   = "Content-Type";
	resp.headers.at(0).value = "text/plain";
	resp.headers.at(1).key   = "X-FANCY";
	resp.headers.at(1).value = "TRUE";
	resp.entity.put("<h1>hello world!</h1>");
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketEstablishmentRequest req){
	const AUTO(shadow_session, boost::make_shared<WebSocketShadowSession>(conn->get_connection_uuid(), Poseidon::Uuid(req.gate_uuid), Poseidon::Uuid(req.client_uuid), STD_MOVE(req.client_ip)));
	WebSocketShadowSessionSupervisor::insert_session(shadow_session);

	Protocol::Box::WebSocketEstablishmentResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketClosureNotification ntfy){
	const AUTO(shadow_session, WebSocketShadowSessionSupervisor::detach_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(shadow_session){
		try {
			shadow_session->on_sync_close(boost::numeric_cast<Poseidon::WebSocket::StatusCode>(ntfy.status_code), ntfy.reason.c_str());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		}
	}

	return 0;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketPackedMessageRequest req){
	bool delivered = false;
	const AUTO(shadow_session, WebSocketShadowSessionSupervisor::get_session(Poseidon::Uuid(req.client_uuid)));
	if(shadow_session){
		try {
			for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
				shadow_session->on_sync_receive(boost::numeric_cast<Poseidon::WebSocket::OpCode>(it->opcode), STD_MOVE(it->payload));
			}
			delivered = true;
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			shadow_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}

	Protocol::Box::WebSocketPackedMessageResponse resp;
	resp.delivered = delivered;
	return resp;
}

}
}
