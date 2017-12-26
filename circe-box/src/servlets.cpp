// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_box.hpp"

#define DEFINE_SERVLET(...)   CIRCE_DEFINE_INTERSERVER_SERVLET(::Circe::Box::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Box {

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::HttpRequest req){
	LOG_CIRCE_FATAL("TODO: CLIENT HTTP REQUEST: ", req);

	Protocol::Box::HttpResponse resp;
	resp.status_code = 200;
	resp.headers.resize(2);
	resp.headers.at(0).key   = "Content-Type";
	resp.headers.at(0).value = "text/plain";
	resp.headers.at(1).key   = "X-FANCY";
	resp.headers.at(1).value = "TRUE";
	resp.entity      = (const unsigned char *)"<h1>hello world!</h1>";
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketEstablishmentRequest req){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET ESTABLISHMENT ", req);

	Protocol::Box::WebSocketEstablishmentResponse resp;
	return resp;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketClosureNotification ntfy){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET CLOSURE ", ntfy);

	return 0;
}

DEFINE_SERVLET(const boost::shared_ptr<Common::InterserverConnection> &conn, Protocol::Box::WebSocketPackedMessageRequest req){
	LOG_CIRCE_FATAL("TODO: CLIENT WEBSOCKET PACKED MESSAGES ", req);

	return 0;
}

}
}
