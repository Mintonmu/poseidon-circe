// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "user_defined_functions.hpp"

// TODO: These are for testing only.
#include "protocol/messages_foyer.hpp"
#include "box_acceptor.hpp"
#include "websocket_shadow_session_supervisor.hpp"
#include "protocol/messages_pilot.hpp"
#include "pilot_connector.hpp"

namespace Circe {
namespace Box {

void User_defined_functions::handle_http_request(
	// Output parameters
	Poseidon::Http::Status_code &resp_status_code,  // This status code is sent to the client intactly.
	Poseidon::Option_map &resp_headers,             // These HTTP headers are sent to the client.
	                                                // The 'Content-Length', 'Connection' and 'Access-Control-*' headers are set by the gate server and shall not be specified here.
	                                                // The gate server compresses HTTP responses as needed. Setting 'Content-Encoding' to 'identity' explicitly suppresses HTTP compression.
	Poseidon::Stream_buffer &resp_entity,           // These data are sent to the client as the response entity.
	// Input parameters
	Poseidon::Uuid client_uuid,                     // This is the UUID of the client connection generated by the gate server.
	std::string client_ip,                          // This is the IP address of the client from the gate server's point of view.
	std::string auth_token,                         // This is the authentication token for this client returned by the auth server.
	Poseidon::Http::Verb verb,                      // This is the verb sent by the client.
	std::string decoded_uri,                        // This is the request URI sent by the client. GET parameters have been stripped.
	Poseidon::Option_map params,                    // These are GET parameters sent by the client (as part of the URI).
	Poseidon::Option_map req_headers,               // These are HTTP headers sent by the client.
	Poseidon::Stream_buffer req_entity)             // This is the request entity sent by the client.
{
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: Handle HTTP request: ", Poseidon::Http::get_string_from_verb(verb), " ", decoded_uri, "\n", params, "\n", req_entity);

	if(decoded_uri == "/favicon.ico"){
		resp_status_code = 404;
		return;
	}

	resp_status_code = 200;
	resp_headers.set(Poseidon::Rcnts::view("Content-Type"), "text/html");
	resp_entity.put("<html>");
	resp_entity.put("  <head>");
	resp_entity.put("    <title>Hello World!</title>");
	resp_entity.put("  </head>");
	resp_entity.put("  <body>");
	resp_entity.put("    <h1>Hello World!</h1>");
	resp_entity.put("  </body>");
	resp_entity.put("</html>");

	(void)resp_status_code;
	(void)resp_headers;
	(void)resp_entity;
	(void)client_uuid;
	(void)client_ip;
	(void)auth_token;
	(void)verb;
	(void)decoded_uri;
	(void)params;
	(void)req_headers;
	(void)req_entity;
}

void User_defined_functions::handle_websocket_establishment(
	const boost::shared_ptr<Websocket_shadow_session> &client_session,  // This is the session cast by the WebSocket connection on the gate server.
	std::string decoded_uri,                                            // This is the request URI sent by the client. GET parameters have been stripped.
	Poseidon::Option_map params)                                        // These are GET parameters sent by the client (as part of the URI).
{
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: Handle WebSocket establishment: ", decoded_uri, "\n", params);
	(void)client_session;
	(void)decoded_uri;
	(void)params;
}

void User_defined_functions::handle_websocket_message(
	const boost::shared_ptr<Websocket_shadow_session> &client_session,  // This is the session cast by the WebSocket connection on the gate server.
	Poseidon::Websocket::Op_code opcode,                                // This is the opcode sent by the client. This may be `Poseidon::Websocket::opcode_data_text` or `Poseidon::Websocket::opcode_data_binary`.
	Poseidon::Stream_buffer payload)                                    // This is the payload sent by the client. If the opcode claims a text message, the payload will be a valid UTF-8 string.
{
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: Handle WebSocket message: ", opcode, ": ", payload);

//	for(unsigned i = 0; i < 3; ++i){
//		char str[100];
//		std::sprintf(str, "hello %d", i);
//		client_session->send(Poseidon::Websocket::op_data_text, Poseidon::Stream_buffer(str));
//	}

	boost::container::vector<boost::shared_ptr<Websocket_shadow_session> > clients;
	Websocket_shadow_session_supervisor::get_all_sessions(clients);

	Protocol::Foyer::Websocket_packed_broadcast_notification_to_gate ntfy;
	for(AUTO(rit, clients.begin()); rit != clients.end(); ++rit){
		ntfy.clients.emplace_back();
		ntfy.clients.back().gate_uuid = (*rit)->get_gate_uuid();
		ntfy.clients.back().client_uuid = (*rit)->get_client_uuid();
	}
	for(unsigned i = 0; i < 3; ++i){
		char str[100];
		std::sprintf(str, "hello %d", i);
		ntfy.messages.emplace_back();
		ntfy.messages.back().opcode = Poseidon::Websocket::opcode_data_text;
		ntfy.messages.back().payload = Poseidon::Stream_buffer(str);
	}
	Box_acceptor::safe_broadcast_notification(ntfy);

	Protocol::Pilot::Compare_exchange_request q;
	q.key = (const unsigned char *)"hello";
	q.criteria.push_back({    "", "foo" });
	q.criteria.push_back({ "foo", "bar" });
	q.criteria.push_back({ "bar",  "kk" });
	q.lock_disposition = Protocol::Pilot::lock_try_acquire_exclusive;
	boost::container::vector<boost::shared_ptr<Common::Interserver_connection> > c;
	Pilot_connector::get_all_clients(c);
	DEBUG_THROW_ASSERT(!c.empty());
	LOG_CIRCE_FATAL(q);
	Protocol::Pilot::Compare_exchange_response r;
	Common::wait_for_response(r, c.at(0)->send_request(q));
	LOG_CIRCE_ERROR(r);

	(void)client_session;
	(void)opcode;
	(void)payload;
}

void User_defined_functions::handle_websocket_closure(
	const boost::shared_ptr<Websocket_shadow_session> &client_session,  // This is the session cast by the WebSocket connection on the gate server.
	Poseidon::Websocket::Status_code status_code,                       // This is the status code in the closure frame received from the client, or `Poseidon::Websocket::status_reserved_abnormal` if no closure frame was received.
	const char *reason)                                                 // This is the payload in the closure frame received from the client, or an unspecified string if no closure frame was received.
{
	PROFILE_ME;

	LOG_CIRCE_FATAL("TODO: Handle WebSocket closure: ", status_code, ": ", reason);
	(void)client_session;
	(void)status_code;
	(void)reason;
}

}
}
