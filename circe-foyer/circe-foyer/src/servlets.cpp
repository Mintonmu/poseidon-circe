// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/messages_foyer.hpp"
#include "protocol/messages_box.hpp"
#include "singletons/box_connector.hpp"
#include "protocol/messages_gate.hpp"
#include "singletons/foyer_acceptor.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Foyer::Servlet_container::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Foyer {

DEFINE_SERVLET_FOR(Protocol::Foyer::Http_request_to_box, connection, req){
	boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > servers_avail;
	Box_connector::get_all_clients(servers_avail);
	CIRCE_PROTOCOL_THROW_UNLESS(servers_avail.size() != 0, Protocol::error_box_connection_lost, Poseidon::Rcnts::view("Connection to box server was lost"));
	const AUTO(box_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	POSEIDON_THROW_ASSERT(box_conn);

	Protocol::Box::Http_request box_req;
	box_req.gate_uuid   = connection->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	box_req.client_ip   = STD_MOVE(req.client_ip);
	box_req.auth_token  = STD_MOVE(req.auth_token);
	box_req.verb        = req.verb;
	box_req.decoded_uri = STD_MOVE(req.decoded_uri);
	box_req.params      = STD_MOVE_IDN(req.params);
	box_req.headers     = STD_MOVE_IDN(req.headers);
	box_req.entity      = STD_MOVE(req.entity);
	CIRCE_LOG_TRACE("Sending request: ", box_req);
	Protocol::Box::Http_response box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	CIRCE_LOG_TRACE("Received response: ", box_resp);

	Protocol::Foyer::Http_response_from_box resp;
	resp.box_uuid    = box_conn->get_connection_uuid();
	resp.status_code = box_resp.status_code;
	resp.headers     = STD_MOVE_IDN(box_resp.headers);
	resp.entity      = STD_MOVE(box_resp.entity);
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_establishment_request_to_box, connection, req){
	boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > servers_avail;
	Box_connector::get_all_clients(servers_avail);
	CIRCE_PROTOCOL_THROW_UNLESS(servers_avail.size() != 0, Protocol::error_box_connection_lost, Poseidon::Rcnts::view("Connection to box server was lost"));
	const AUTO(box_conn, servers_avail.at(Poseidon::random_uint32() % servers_avail.size()));
	POSEIDON_THROW_ASSERT(box_conn);

	Protocol::Box::Websocket_establishment_request box_req;
	box_req.gate_uuid   = connection->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	box_req.client_ip   = STD_MOVE(req.client_ip);
	box_req.auth_token  = STD_MOVE(req.auth_token);
	box_req.decoded_uri = STD_MOVE(req.decoded_uri);
	box_req.params      = STD_MOVE_IDN(req.params);
	CIRCE_LOG_TRACE("Sending request: ", box_req);
	Protocol::Box::Websocket_establishment_response box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	CIRCE_LOG_TRACE("Received response: ", box_resp);

	Protocol::Foyer::Websocket_establishment_response_from_box resp;
	resp.box_uuid = box_conn->get_connection_uuid();
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_closure_notification_to_box, connection, ntfy){
	const AUTO(box_conn, Box_connector::get_client(Poseidon::Uuid(ntfy.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::error_box_connection_lost, Poseidon::Rcnts::view("Connection to box server was lost"));

	Protocol::Box::Websocket_closure_notification box_ntfy;
	box_ntfy.gate_uuid   = connection->get_connection_uuid();
	box_ntfy.client_uuid = ntfy.client_uuid;
	box_ntfy.status_code = ntfy.status_code;
	box_ntfy.reason      = STD_MOVE(ntfy.reason);
	CIRCE_LOG_TRACE("Sending notification: ", box_ntfy);
	box_conn->send_notification(box_ntfy);

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_kill_notification_to_gate, /*connection*/, ntfy){
	const AUTO(gate_conn, Foyer_acceptor::get_session(Poseidon::Uuid(ntfy.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::error_gate_connection_lost, Poseidon::Rcnts::view("The gate server specified was not found"));

	Protocol::Gate::Websocket_kill_notification gate_ntfy;
	gate_ntfy.client_uuid = ntfy.client_uuid;
	gate_ntfy.status_code = ntfy.status_code;
	gate_ntfy.reason      = STD_MOVE(ntfy.reason);
	CIRCE_LOG_TRACE("Sending notification: ", gate_ntfy);
	gate_conn->send_notification(gate_ntfy);

	return Protocol::error_success;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_packed_message_request_to_box, connection, req){
	const AUTO(box_conn, Box_connector::get_client(Poseidon::Uuid(req.box_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(box_conn, Protocol::error_box_connection_lost, Poseidon::Rcnts::view("Connection to box server was lost"));

	Protocol::Box::Websocket_packed_message_request box_req;
	box_req.gate_uuid   = connection->get_connection_uuid();
	box_req.client_uuid = req.client_uuid;
	for(AUTO(it, req.messages.begin()); it != req.messages.end(); ++it){
		Protocol::Common_websocket_frame frame;
		frame.opcode  = it->opcode;
		frame.payload = STD_MOVE(it->payload);
		box_req.messages.push_back(STD_MOVE(frame));
	}
	CIRCE_LOG_TRACE("Sending request: ", box_req);
	Protocol::Box::Websocket_packed_message_response box_resp;
	Common::wait_for_response(box_resp, box_conn->send_request(box_req));
	CIRCE_LOG_TRACE("Received response: ", box_resp);

	Protocol::Foyer::Websocket_packed_message_response_from_box resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_packed_message_request_to_gate, /*connection*/, req){
	const AUTO(gate_conn, Foyer_acceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::error_gate_connection_lost, Poseidon::Rcnts::view("The gate server specified was not found"));

	Protocol::Gate::Websocket_packed_message_request gate_req;
	gate_req.client_uuid = req.client_uuid;
	gate_req.messages    = STD_MOVE(req.messages);
	CIRCE_LOG_TRACE("Sending request: ", gate_req);
	Protocol::Gate::Websocket_packed_message_response gate_resp;
	Common::wait_for_response(gate_resp, gate_conn->send_request(gate_req));
	CIRCE_LOG_TRACE("Received response: ", gate_resp);

	Protocol::Foyer::Websocket_packed_message_response_from_gate resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Check_gate_request, /*connection*/, req){
	const AUTO(gate_conn, Foyer_acceptor::get_session(Poseidon::Uuid(req.gate_uuid)));
	CIRCE_PROTOCOL_THROW_UNLESS(gate_conn, Protocol::error_gate_connection_lost, Poseidon::Rcnts::view("The gate server specified was not found"));

	Protocol::Foyer::Check_gate_response resp;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Foyer::Websocket_packed_broadcast_notification_to_gate, /*connection*/, ntfy){
	boost::container::flat_map<Poseidon::Uuid,
		std::pair<boost::shared_ptr<Common::Interserver_connection>, // gate_conn
		          boost::container::flat_set<Poseidon::Uuid> >      // clients
		> gate_servers;
	gate_servers.reserve(ntfy.clients.size());

	// Collect gate servers.
	for(AUTO(qcit, ntfy.clients.begin()); qcit != ntfy.clients.end(); ++qcit){
		const AUTO(gspair, gate_servers.emplace(Poseidon::Uuid(qcit->gate_uuid), VALUE_TYPE(gate_servers)::mapped_type()));
		// Get the session for this gate server if a new element has just been inserted.
		AUTO_REF(gate_conn, gspair.first->second.first);
		if(gspair.second){
			gate_conn = Foyer_acceptor::get_session(gspair.first->first);
		}
		// Ignore all clients on a gate server that could not be found.
		if(!gate_conn){
			CIRCE_LOG_TRACE("> Gate server not found: ", gspair.first->first);
			continue;
		}
		gspair.first->second.second.insert(Poseidon::Uuid(qcit->client_uuid));
	}

	Protocol::Gate::Websocket_packed_broadcast_notification gate_ntfy;
	// Send messages to gate servers.
	for(AUTO(gsit, gate_servers.begin()); gsit != gate_servers.end(); ++gsit){
		const AUTO_REF(gate_conn, gsit->second.first);
		if(!gate_conn){
			continue;
		}
		try {
			// Fill in UUIDs of target clients.
			gate_ntfy.clients.clear();
			for(AUTO(cit, gsit->second.second.begin()); cit != gsit->second.second.end(); ++cit){
				gate_ntfy.clients.emplace_back();
				gate_ntfy.clients.back().client_uuid = *cit;
			}
			// Fill in the message body. Note that this happens at most once.
			if(!ntfy.messages.empty()){
				gate_ntfy.messages = STD_MOVE(ntfy.messages);
				ntfy.messages.clear();
			}
			gate_conn->send_notification(gate_ntfy);
		} catch(std::exception &e){
			CIRCE_LOG_ERROR("std::exception thrown: what = ", e.what());
			gate_conn->shutdown(Protocol::error_internal_error, e.what());
		}
	}

	return Protocol::error_success;
}

}
}
