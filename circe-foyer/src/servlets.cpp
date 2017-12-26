// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
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
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::Box::HttpResponse box_resp;
	{
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
		Common::wait_for_response(box_resp, box_conn->send_request(box_req));
		LOG_CIRCE_TRACE("Received response: ", box_resp);
	}

	Protocol::Foyer::HttpResponseFromBox resp;
	resp.status_code = box_resp.status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(box_resp.headers));
	resp.entity      = STD_MOVE(box_resp.entity);
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::WebSocketEstablishmentRequestToBox req){
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::Box::WebSocketEstablishmentResponse box_resp;
	{
		Protocol::Box::WebSocketEstablishmentRequest box_req;
		box_req.gate_uuid   = conn->get_connection_uuid();
		box_req.client_uuid = req.client_uuid;
		box_req.client_ip   = STD_MOVE(req.client_ip);
		box_req.auth_token  = STD_MOVE(req.auth_token);
		box_req.decoded_uri = STD_MOVE(req.decoded_uri);
		Protocol::copy_key_values(box_req.params, STD_MOVE(req.params));
		LOG_CIRCE_TRACE("Sending request: ", box_req);
		Common::wait_for_response(box_resp, box_conn->send_request(box_req));
		LOG_CIRCE_TRACE("Received response: ", box_resp);
	}

	Protocol::Foyer::WebSocketEstablishmentResponseFromBox resp;
	resp.status_code = box_resp.status_code;
	resp.reason      = STD_MOVE(box_resp.reason);
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Foyer::WebSocketClosureNotificationToBox ntfy){
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

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
	DEBUG_THROW_UNLESS(gate_conn, Poseidon::Cbpp::Exception, Protocol::ERR_GATE_NOT_FOUND, Poseidon::sslit("The specified gate server was not found"));

	Protocol::Gate::WebSocketKillResponse gate_resp;
	{
		Protocol::Gate::WebSocketKillRequest gate_req;
		gate_req.client_uuid = req.client_uuid;
		gate_req.status_code = req.status_code;
		gate_req.reason      = STD_MOVE(req.reason);
		LOG_CIRCE_TRACE("Sending request: ", gate_req);
		Common::wait_for_response(gate_resp, gate_conn->send_request(gate_req));
		LOG_CIRCE_TRACE("Received response: ", gate_resp);
	}

	Protocol::Foyer::WebSocketKillResponseFromGate resp;
	return resp;
}

}
}
