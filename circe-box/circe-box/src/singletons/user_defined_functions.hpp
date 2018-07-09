// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_BOX_SINGLETONS_USER_DEFINED_FUNCTIONS_HPP_
#define CIRCE_BOX_SINGLETONS_USER_DEFINED_FUNCTIONS_HPP_

#include <poseidon/uuid.hpp>
#include <poseidon/option_map.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/http/verbs.hpp>
#include <poseidon/http/status_codes.hpp>
#include <poseidon/websocket/opcodes.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include "../websocket_shadow_session.hpp"

namespace Circe {
namespace Box {

// See `user_defined_functions.cpp` for comments.

class User_defined_functions {
private:
	User_defined_functions();

public:
	static Poseidon::Http::Status_code handle_http_request(
		// Output parameters
		Poseidon::Option_map &resp_headers,
		Poseidon::Stream_buffer &resp_entity,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		std::string auth_token,
		Poseidon::Http::Verb verb,
		std::string decoded_uri,
		Poseidon::Option_map params,
		Poseidon::Option_map req_headers,
		Poseidon::Stream_buffer req_entity);

	static void handle_websocket_establishment(
		const boost::shared_ptr<Websocket_shadow_session> &client_session,
		std::string decoded_uri,
		Poseidon::Option_map params);

	static void handle_websocket_message(
		const boost::shared_ptr<Websocket_shadow_session> &client_session,
		Poseidon::Websocket::Opcode opcode,
		Poseidon::Stream_buffer payload);

	static void handle_websocket_closure(
		const boost::shared_ptr<Websocket_shadow_session> &client_session,
		Poseidon::Websocket::Status_code status_code,
		const char *reason);
};

}
}

#endif
