// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_USER_DEFINED_FUNCTIONS_HPP_
#define CIRCE_GATE_USER_DEFINED_FUNCTIONS_HPP_

#include <poseidon/uuid.hpp>
#include <poseidon/optional_map.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/http/verbs.hpp>
#include <poseidon/http/status_codes.hpp>
#include <poseidon/websocket/opcodes.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include "websocket_shadow_session.hpp"

namespace Circe {
namespace Box {

// See `user_defined_functions.cpp` for comments.

class UserDefinedFunctions {
private:
	UserDefinedFunctions();

public:
	static void handle_http_request(
		// Output parameters
		Poseidon::Http::StatusCode &resp_status_code,
		Poseidon::OptionalMap &resp_headers,
		Poseidon::StreamBuffer &resp_entity,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		std::string auth_token,
		Poseidon::Http::Verb verb,
		std::string decoded_uri,
		Poseidon::OptionalMap params,
		Poseidon::OptionalMap req_headers,
		Poseidon::StreamBuffer req_entity);

	static void handle_websocket_establishment(
		const boost::shared_ptr<WebSocketShadowSession> &client_session,
		std::string decoded_uri,
		Poseidon::OptionalMap params);

	static void handle_websocket_message(
		const boost::shared_ptr<WebSocketShadowSession> &client_session,
		Poseidon::WebSocket::OpCode opcode,
		Poseidon::StreamBuffer payload);

	static void handle_websocket_closure(
		const boost::shared_ptr<WebSocketShadowSession> &client_session,
		Poseidon::WebSocket::StatusCode status_code,
		const char *reason);
};

}
}

#endif
