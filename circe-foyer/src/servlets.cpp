// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/box_connector.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/foyer_acceptor.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Foyer::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Foyer {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::HttpRequestToBox req){
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

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::WebSocketEstablishmentRequestToBox req){
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
	resp.box_uuid    = box_conn->get_connection_uuid();
	resp.status_code = box_resp.status_code;
	resp.reason      = STD_MOVE(box_resp.reason);
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::WebSocketClosureNotificationToBox ntfy){
	const AUTO(box_conn, BoxConnector::get_client(Poseidon::Uuid(ntfy.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	{
		Protocol::Box::WebSocketClosureNotification box_ntfy;
		box_ntfy.gate_uuid   = conn->get_connection_uuid();
		box_ntfy.client_uuid = ntfy.client_uuid;
		box_ntfy.status_code = ntfy.status_code;
		box_ntfy.reason      = STD_MOVE(ntfy.reason);
		LOG_CIRCE_TRACE("Sending notification: ", box_ntfy);
		box_conn->send_notification(box_ntfy);
	}

	return 0;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Foyer::WebSocketKillRequestToGate req){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Gate::WebSocketKillRequest gate_req;
	gate_req.client_uuid = req.client_uuid;
	gate_req.status_code = req.status_code;
	gate_req.reason      = STD_MOVE(req.reason);
	LOG_CIRCE_TRACE("Sending request: ", gate_req);
	Protocol::Gate::WebSocketKillResponse gate_resp;
	Common::wait_for_response(gate_resp, gate_conn->send_request(gate_req));
	LOG_CIRCE_TRACE("Received response: ", gate_resp);

	Protocol::Foyer::WebSocketKillResponseFromGate resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::WebSocketPackedMessageRequestToBox req){
	const AUTO(box_conn, BoxConnector::get_client(Poseidon::Uuid(req.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::Box::WebSocketPackedMessageRequest box_req;
	box_req.gate_uuid   = conn->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	for(AUTO(rit, req.messages.begin()); rit != req.messages.end(); ++rit){
		AUTO(wit, box_req.messages.emplace(box_req.messages.end()));
		wit->opcode  = rit->opcode;
		wit->payload = STD_MOVE(rit->payload);
	}
	LOG_CIRCE_TRACE("Sending request: ", box_req);
	Protocol::Box::WebSocketPackedMessageResponse box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	LOG_CIRCE_TRACE("Received response: ", box_resp);

	Protocol::Foyer::WebSocketPackedMessageResponseFromBox resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Foyer::WebSocketPackedMessageRequestToGate req){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Gate::WebSocketPackedMessageRequest gate_req;
	gate_req.client_uuid = req.client_uuid;
	for(AUTO(rit, req.messages.begin()); rit != req.messages.end(); ++rit){
		AUTO(wit, gate_req.messages.emplace(gate_req.messages.end()));
		wit->opcode  = rit->opcode;
		wit->payload = STD_MOVE(rit->payload);
	}
	LOG_CIRCE_TRACE("Sending request: ", gate_req);
	Protocol::Gate::WebSocketPackedMessageResponse gate_resp;
	Common::wait_for_response(gate_resp, gate_conn->send_request(gate_req));
	LOG_CIRCE_TRACE("Received response: ", gate_resp);

	Protocol::Foyer::WebSocketPackedMessageResponseFromGate resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &/*conn*/, Protocol::Foyer::CheckGateRequest req){
	const AUTO(gate_conn, FoyerAcceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::ERR_GATE_CONNECTION_LOST, Poseidon::sslit("The gate server specified was not found"));

	Protocol::Foyer::CheckGateResponse resp;
	return resp;
}

}
}
