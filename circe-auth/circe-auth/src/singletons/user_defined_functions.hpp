// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_AUTH_SINGLETONS_USER_DEFINED_FUNCTIONS_HPP_
#define CIRCE_AUTH_SINGLETONS_USER_DEFINED_FUNCTIONS_HPP_

#include <poseidon/uuid.hpp>
#include <poseidon/option_map.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/http/verbs.hpp>
#include <poseidon/http/status_codes.hpp>
#include <poseidon/websocket/opcodes.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include <boost/container/deque.hpp>

namespace Circe {
namespace Auth {

// See `user_defined_functions.cpp` for comments.

class User_defined_functions {
private:
	User_defined_functions();

public:
	static std::string check_http_authentication(
		// Output parameters
		Poseidon::Http::Status_code &resp_status_code,
		Poseidon::Option_map &resp_headers,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		Poseidon::Http::Verb verb,
		std::string decoded_uri,
		Poseidon::Option_map params,
		Poseidon::Option_map req_headers);

	static std::string check_websocket_authentication(
		// Output parameters
		boost::container::deque<std::pair<Poseidon::Websocket::Op_code,
			Poseidon::Stream_buffer> > &resp_messages,
		Poseidon::Websocket::Status_code &resp_status_code,
		std::string &resp_reason,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		std::string decoded_uri,
		Poseidon::Option_map params);
};

}
}

#endif
