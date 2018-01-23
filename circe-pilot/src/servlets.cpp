// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_pilot.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Pilot::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Pilot {

/*DEFINE_SERVLET_FOR(Protocol::Pilot::HttpRequestToBox, conn, req){
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

	Protocol::Pilot::HttpResponseFromBox resp;
	resp.box_uuid    = box_conn->get_connection_uuid();
	resp.status_code = box_resp.status_code;
	Protocol::copy_key_values(resp.headers, STD_MOVE(box_resp.headers));
	resp.entity      = STD_MOVE(box_resp.entity);
	return resp;
}*/

}
}
