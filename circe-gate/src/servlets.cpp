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
#include "client_websocket_session.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Gate::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Gate {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketKillRequest req){
	const AUTO(http_session, ClientHttpAcceptor::get_session(Poseidon::Uuid(req.client_uuid)));
	DEBUG_THROW_UNLESS(http_session, Poseidon::Cbpp::Exception, Protocol::ERR_CLIENT_NOT_FOUND, Poseidon::sslit("The specified WebSocket client was not found"));
	const AUTO(ws_session, boost::dynamic_pointer_cast<ClientWebSocketSession>(http_session->get_upgraded_session()));
	DEBUG_THROW_UNLESS(ws_session, Poseidon::Cbpp::Exception, Protocol::ERR_CLIENT_NOT_FOUND, Poseidon::sslit("The specified WebSocket client was not found"));

	LOG_CIRCE_INFO("Killing client WebSocket session: remote = ", ws_session->get_remote_info(), ", req = ", req);
	ws_session->shutdown(req.status_code, req.reason.c_str());

	Protocol::Gate::WebSocketKillResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Gate::WebSocketPackedMessageRequest req){
	const AUTO(http_session, ClientHttpAcceptor::get_session(Poseidon::Uuid(req.client_uuid)));
	DEBUG_THROW_UNLESS(http_session, Poseidon::Cbpp::Exception, Protocol::ERR_CLIENT_NOT_FOUND, Poseidon::sslit("The specified WebSocket client was not found"));
	const AUTO(ws_session, boost::dynamic_pointer_cast<ClientWebSocketSession>(http_session->get_upgraded_session()));
	DEBUG_THROW_UNLESS(ws_session, Poseidon::Cbpp::Exception, Protocol::ERR_CLIENT_NOT_FOUND, Poseidon::sslit("The specified WebSocket client was not found"));

	for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
		ws_session->send(it->opcode, Poseidon::StreamBuffer(it->payload));
	}

	Protocol::Gate::WebSocketPackedMessageResponse resp;
	return resp;
}

}
}
