// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_gate_foyer.hpp"
#include "protocol/messages_foyer_box.hpp"
#include "singletons/box_connector.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Foyer::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Foyer {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &gate_conn, Protocol::GF_ProcessHttpRequest gate_req){
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::BF_ReturnHttpResponse box_resp;
	{
		Protocol::FB_ProcessHttpRequest box_req;
		box_req.gate_uuid   = gate_conn->get_connection_uuid();
		box_req.client_uuid = gate_req.client_uuid;
		box_req.client_ip   = STD_MOVE(gate_req.client_ip);
		box_req.auth_token  = STD_MOVE(gate_req.auth_token);
		box_req.verb        = gate_req.verb;
		box_req.decoded_uri = STD_MOVE(gate_req.decoded_uri);
		Protocol::copy_key_values(box_req.params, STD_MOVE(gate_req.params));
		Protocol::copy_key_values(box_req.headers, STD_MOVE(gate_req.headers));
		box_req.entity      = STD_MOVE(gate_req.entity);
		LOG_CIRCE_TRACE("Sending request: ", box_req);
		Common::wait_for_response(box_resp, box_conn->send_request(box_req));
		LOG_CIRCE_TRACE("Received response: ", box_resp);
	}

	Protocol::FG_ReturnHttpResponse gate_resp;
	gate_resp.status_code = box_resp.status_code;
	Protocol::copy_key_values(gate_resp.headers, STD_MOVE(box_resp.headers));
	gate_resp.entity      = STD_MOVE(box_resp.entity);
	return gate_resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &gate_conn, Protocol::GF_EstablishWebSocketConnection gate_req){
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	Protocol::BF_ReturnWebSocketEstablishmentResult box_resp;
	{
		Protocol::FB_EstablishWebSocketConnection box_req;
		box_req.gate_uuid   = gate_conn->get_connection_uuid();
		box_req.client_uuid = gate_req.client_uuid;
		box_req.client_ip   = STD_MOVE(gate_req.client_ip);
		box_req.auth_token  = STD_MOVE(gate_req.auth_token);
		box_req.decoded_uri = STD_MOVE(gate_req.decoded_uri);
		Protocol::copy_key_values(box_req.params, STD_MOVE(gate_req.params));
		LOG_CIRCE_TRACE("Sending request: ", box_req);
		Common::wait_for_response(box_resp, box_conn->send_request(box_req));
		LOG_CIRCE_TRACE("Received response: ", box_resp);
	}

	Protocol::FG_ReturnWebSocketEstablishmentResult gate_resp;
	gate_resp.status_code = box_resp.status_code;
	gate_resp.message     = STD_MOVE(box_resp.message);
	return gate_resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &gate_conn, Protocol::GF_NotifyWebSocketClosure gate_ntfy){
	const AUTO(box_conn, BoxConnector::get_connection());
	DEBUG_THROW_UNLESS(box_conn, Poseidon::Cbpp::Exception, Protocol::ERR_BOX_CONNECTION_LOST, Poseidon::sslit("Connection to box server was lost"));

	{
		Protocol::FB_NotifyWebSocketClosure box_ntfy;
		box_ntfy.gate_uuid   = gate_conn->get_connection_uuid();
		box_ntfy.client_uuid = gate_ntfy.client_uuid;
		box_ntfy.status_code = gate_ntfy.status_code;
		box_ntfy.message     = STD_MOVE(gate_ntfy.message);
		LOG_CIRCE_TRACE("Sending notification: ", box_ntfy);
		box_conn->send_notification(box_ntfy);
	}

	return 0;
}

}
}
