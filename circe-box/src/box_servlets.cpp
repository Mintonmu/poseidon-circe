// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/box_acceptor.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_foyer_box.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Box::BoxAcceptor::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET_FOR(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn, Protocol::FB_ProcessHttpRequest foyer_req){
	LOG_CIRCE_FATAL("TODO: CLIENT HTTP REQUEST: ", foyer_req);

	Protocol::BF_ReturnHttpResponse foyer_resp;
	foyer_resp.status_code = 200;
	foyer_resp.headers.resize(2);
	foyer_resp.headers.at(0).key   = "Content-Type";
	foyer_resp.headers.at(0).value = "text/plain";
	foyer_resp.headers.at(1).key   = "X-FANCY";
	foyer_resp.headers.at(1).value = "TRUE";
	foyer_resp.entity      = (const unsigned char *)"<h1>hello world!</h1>";
	return foyer_resp;
}

DEFINE_SERVLET_FOR(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn, Protocol::FB_EstablishWebSocketConnection foyer_req){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET ESTABLISHMENT ", foyer_req);

	Protocol::BF_ReturnWebSocketEstablishmentResult foyer_resp;
	return foyer_resp;
}

DEFINE_SERVLET_FOR(const boost::shared_ptr<Common::InterserverConnection> &foyer_conn, Protocol::FB_NotifyWebSocketClosure foyer_req){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET CLOSURE ", foyer_req);

	return 0;
}

}
}
