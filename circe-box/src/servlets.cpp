// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_foyer_box.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Box::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn, Protocol::FB_HttpRequest foyer_req){
	LOG_CIRCE_FATAL("TODO: CLIENT HTTP REQUEST: ", foyer_req);

	Protocol::BF_HttpResponse foyer_resp;
	foyer_resp.status_code = 200;
	foyer_resp.headers.resize(2);
	foyer_resp.headers.at(0).key   = "Content-Type";
	foyer_resp.headers.at(0).value = "text/plain";
	foyer_resp.headers.at(1).key   = "X-FANCY";
	foyer_resp.headers.at(1).value = "TRUE";
	foyer_resp.entity      = (const unsigned char *)"<h1>hello world!</h1>";
	return foyer_resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn, Protocol::FB_WebSocketEstablishmentRequest foyer_req){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET ESTABLISHMENT ", foyer_req);

	Protocol::BF_WebSocketEstablishmentResponse foyer_resp;
	return foyer_resp;
}

}
}
