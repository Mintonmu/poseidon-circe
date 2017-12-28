// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/client_session_container.hpp"

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
	LOG_CIRCE_INFO("Creating ClientWebSocketShadowSession: ", req);
	const AUTO(session, boost::make_shared<ClientWebSocketShadowSession>(conn->get_connection_uuid(), Poseidon::Uuid(req.gate_uuid), Poseidon::Uuid(req.client_uuid), req.client_ip, req.auth_token));
	session->on_sync_open();
	ClientSessionContainer::insert_session(session);

	Protocol::Box::WebSocketEstablishmentResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Box::WebSocketClosureNotification ntfy){
	const AUTO(session, ClientSessionContainer::get_session(Poseidon::Uuid(ntfy.client_uuid)));
	DEBUG_THROW_UNLESS(session, Poseidon::Cbpp::Exception, Protocol::ERR_CLIENT_SHADOW_SESSION_NOT_FOUND, Poseidon::sslit("ClientWebSocketShadowSession not found"));

	LOG_CIRCE_INFO("Removing ClientWebSocketShadowSession: ", ntfy);
	ClientSessionContainer::remove_session(Poseidon::Uuid(ntfy.client_uuid), boost::numeric_cast<Poseidon::WebSocket::StatusCode>(ntfy.status_code), ntfy.reason.c_str());

	return 0;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Box::WebSocketPackedMessageRequest req){
	bool delivered = false;
	const AUTO(session, ClientSessionContainer::get_session(Poseidon::Uuid(req.client_uuid)));
	if(session){
		try {
			for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
				session->on_sync_receive(boost::numeric_cast<Poseidon::WebSocket::OpCode>(it->opcode), STD_MOVE(it->payload));
			}
			delivered = true;
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: what = ", e.what());
			session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}

	Protocol::Box::WebSocketPackedMessageResponse resp;
	resp.delivered = delivered;
	return resp;
}

}
}
