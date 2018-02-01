// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/client_http_acceptor.hpp"
#include "singletons/ip_ban_list.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Gate::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Gate {

DEFINE_SERVLET_FOR(Protocol::Gate::WebSocketKillNotification, /*connection*/, ntfy){
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(ntfy.client_uuid)));
	if(ws_session){
		LOG_CIRCE_INFO("Killing client WebSocket session: remote = ", ws_session->get_remote_info(), ", ntfy = ", ntfy);
		ws_session->shutdown(boost::numeric_cast<Poseidon::WebSocket::StatusCode>(ntfy.status_code), ntfy.reason.c_str());
	}

	return Protocol::ERR_SUCCESS;
}

DEFINE_SERVLET_FOR(Protocol::Gate::WebSocketPackedMessageRequest, /*connection*/, req){
	const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(req.client_uuid)));
	if(ws_session){
		try {
			for(AUTO(qmit, req.messages.begin()); qmit != req.messages.end(); ++qmit){
				ws_session->send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(qmit->opcode), STD_MOVE(qmit->payload));
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
		}
	}

	Protocol::Gate::WebSocketPackedMessageResponse resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Gate::WebSocketPackedBroadcastNotification, /*connection*/, req){
	for(AUTO(qcit, req.clients.begin()); qcit != req.clients.end(); ++qcit){
		const AUTO(ws_session, ClientHttpAcceptor::get_websocket_session(Poseidon::Uuid(qcit->client_uuid)));
		if(ws_session){
			try {
				for(AUTO(qmit, req.messages.begin()); qmit != req.messages.end(); ++qmit){
					ws_session->send(boost::numeric_cast<Poseidon::WebSocket::OpCode>(qmit->opcode), qmit->payload);
				}
			} catch(std::exception &e){
				LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
				ws_session->shutdown(Poseidon::WebSocket::ST_INTERNAL_ERROR, e.what());
			}
		}
	}

	return Protocol::ERR_SUCCESS;
}

DEFINE_SERVLET_FOR(Protocol::Gate::UnbanIpRequest, /*connection*/, req){
	const bool found = IpBanList::remove_ban(req.client_ip.c_str());

	Protocol::Gate::UnbanIpResponse resp;
	resp.found = found;
	return resp;
}

}
}
