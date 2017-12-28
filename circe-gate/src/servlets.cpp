// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/client_http_acceptor.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Gate::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Gate {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketKillRequest req){
	bool found = false;
	bool killed = false;
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		LOG_CIRCE_INFO("Killing client WebSocket session: remote = ", ws_session->get_remote_info(), ", req = ", req);
		found = true;
		killed = ws_session->shutdown(boost::numeric_cast<Poseidon::WebSocket::StatusCode>(req.status_code), req.reason.c_str());
	}

	Protocol::Gate::WebSocketKillResponse resp;
	resp.found  = found;
	resp.killed = killed;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketPackedMessageRequest req){
	bool delivered = false;
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		delivered = true;
		for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
			ws_session->send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(it->opcode), Poseidon::StreamBuffer(it->payload));
		}
	}

	Protocol::Gate::WebSocketPackedMessageResponse resp;
	resp.delivered = delivered;
	return resp;
}

}
}
