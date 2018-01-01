// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/client_http_acceptor.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Gate::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Gate {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketKillRequest req){
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		LOG_CIRCE_INFO("Killing client WebSocket session: remote = ", ws_session->get_remote_info(), ", req = ", req);
		ws_session->shutdown(boost::numeric_cast<Poseidon::WebSocket::StatusCode>(req.status_code), req.reason.c_str());
	}

	Protocol::Gate::WebSocketKillResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketPackedMessageRequest req){
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		try {
			while(!req.messages.empty()){
				ws_session->send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(req.messages.front().opcode), STD_MOVE(req.messages.front().payload));
				req.messages.pop_front();
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}

	Protocol::Gate::WebSocketPackedMessageResponse resp;
	return resp;
}

}
}
