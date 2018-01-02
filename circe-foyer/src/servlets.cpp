// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/box_connector.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/foyer_acceptor.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Foyer::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Foyer {

DEFINE_SERVLET_FOR(Protocol::Foyer::HttpRequestToBox, conn, req){
	boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > servers_avail;
	BoxConnector::get_all_clients(servers_avail);
	CIRCE_PROTOCOL_THROW_UNLESS(servers_avail.size() != 0, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));
	const AUTO(box_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(box_conn);

	Protocol::Box::HttpRequest box_req;
	box_req.gate_uuid   = conn->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	box_req.client_ip   = STD_MOVE(req.client_ip);
	box_req.auth_token  = STD_MOVE(req.auth_token);
	box_req.verb        = req.verb;
	box_req.decoded_uri = STD_MOVE(req.decoded_uri);
	Protocol::copy_key_values(box_req.params, STD_MOVE(req.params));
	Protocol::copy_key_values(box_req.headers, STD_MOVE(req.headers));
	box_req.entity      = STD_MOVE(req.entity);
	LOG_CIRCE_TRACE("Sending request: ", box_req);
	Protocol::Box::HttpResponse box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	LOG_CIRCE_TRACE("Received response: ", box_resp);

	Protocol::Foyer::HttpResponseFromBox resp;
	resp.box_uuid    = box_conn->get_connection_uuid();
	resp.status_code = box_resp.status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(box_resp.headers));
	resp.entity      = STD_MOVE(box_resp.entity);
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::WebSocketEstablishmentRequestToBox, conn, req){
	boost::container::vector<boost::shared_ptr<Common::InterserverConnection> > servers_avail;
	BoxConnector::get_all_clients(servers_avail);
	CIRCE_PROTOCOL_THROW_UNLESS(servers_avail.size() != 0, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));
	const AUTO(box_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	DEBUG_THROW_ASSERT(box_conn);

	Protocol::Box::WebSocketEstablishmentRequest box_req;
	box_req.gate_uuid   = conn->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	box_req.client_ip   = STD_MOVE(req.client_ip);
	box_req.auth_token  = STD_MOVE(req.auth_token);
	box_req.decoded_uri = STD_MOVE(req.decoded_uri);
	Protocol::copy_key_values(box_req.params, STD_MOVE(req.params));
	LOG_CIRCE_TRACE("Sending request: ", box_req);
	Protocol::Box::WebSocketEstablishmentResponse box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	LOG_CIRCE_TRACE("Received response: ", box_resp);

	Protocol::Foyer::WebSocketEstablishmentResponseFromBox resp;
	resp.box_uuid = box_conn->get_connection_uuid();
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::WebSocketClosureNotificationToBox, conn, ntfy){
	const AUTO(box_conn, BoxConnector::get_client(Poseidon::Uuid(ntfy.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::Box::WebSocketClosureNotification box_ntfy;
	box_ntfy.gate_uuid   = conn->get_connection_uuid();
	box_ntfy.client_uuid = ntfy.client_uuid;
	box_ntfy.status_code = ntfy.status_code;
	box_ntfy.reason      = STD_MOVE(ntfy.reason);
	LOG_CIRCE_TRACE("Sending notification: ", box_ntfy);
	box_conn->send_notification(box_ntfy);

	return 0;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::WebSocketKillNotificationToGate, /*conn*/, ntfy){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(ntfy.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Gate::WebSocketKillNotification gate_ntfy;
	gate_ntfy.client_uuid = ntfy.client_uuid;
	gate_ntfy.status_code = ntfy.status_code;
	gate_ntfy.reason      = STD_MOVE(ntfy.reason);
	LOG_CIRCE_TRACE("Sending notification: ", gate_ntfy);
	gate_conn->send_notification(gate_ntfy);

	return 0;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::WebSocketPackedMessageRequestToBox, conn, req){
	const AUTO(box_conn, BoxConnector::get_client(Poseidon::Uuid(req.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::Box::WebSocketPackedMessageRequest box_req;
	box_req.gate_uuid   = conn->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	while(!req.messages.empty()){
		box_req.messages.emplace_back();
		box_req.messages.back().opcode  = STD_MOVE(req.messages.front().opcode);
		box_req.messages.back().payload = STD_MOVE(req.messages.front().payload);
		req.messages.pop_front();
	}
	LOG_CIRCE_TRACE("Sending request: ", box_req);
	Protocol::Box::WebSocketPackedMessageResponse box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	LOG_CIRCE_TRACE("Received response: ", box_resp);

	Protocol::Foyer::WebSocketPackedMessageResponseFromBox resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::WebSocketPackedMessageRequestToGate, /*conn*/, req){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Gate::WebSocketPackedMessageRequest gate_req;
	gate_req.client_uuid = req.client_uuid;
	while(!req.messages.empty()){
		gate_req.messages.emplace_back();
		gate_req.messages.back().opcode  = STD_MOVE(req.messages.front().opcode);
		gate_req.messages.back().payload = STD_MOVE(req.messages.front().payload);
		req.messages.pop_front();
	}
	LOG_CIRCE_TRACE("Sending request: ", gate_req);
	Protocol::Gate::WebSocketPackedMessageResponse gate_resp;
	Common::wait_for_response(gate_resp, gate_conn->send_request(gate_req));
	LOG_CIRCE_TRACE("Received response: ", gate_resp);

	Protocol::Foyer::WebSocketPackedMessageResponseFromGate resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::CheckGateRequest, /*conn*/, req){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Foyer::CheckGateResponse resp;
	return resp;
}

}
}
