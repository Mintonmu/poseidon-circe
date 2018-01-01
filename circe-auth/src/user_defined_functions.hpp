// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_AUTH_USER_DEFINED_FUNCTIONS_HPP_
#define CIRCE_AUTH_USER_DEFINED_FUNCTIONS_HPP_

#include <poseidon/uuid.hpp>
#include <poseidon/optional_map.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/http/verbs.hpp>
#include <poseidon/http/status_codes.hpp>
#include <poseidon/websocket/opcodes.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include <boost/container/deque.hpp>

namespace Circe {
namespace Auth {

// See `user_defined_functions.cpp` for comments.

class UserDefinedFunctions {
private:
	UserDefinedFunctions();

public:
	static std::string check_http_authentication(
		// Output parameters
		Poseidon::Http::StatusCode &resp_status_code,
		Poseidon::OptionalMap &resp_headers,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		Poseidon::Http::Verb verb,
		std::string decoded_uri,
		Poseidon::OptionalMap params,
		Poseidon::OptionalMap req_headers);

	static std::string check_websocket_authentication(
		// Output parameters
		boost::container::deque<std::pair<Poseidon::WebSocket::OpCode,
			Poseidon::StreamBuffer> > &resp_messages,
		Poseidon::WebSocket::StatusCode &resp_status_code,
		std::string &resp_reason,
		// Input parameters
		Poseidon::Uuid client_uuid,
		std::string client_ip,
		std::string decoded_uri,
		Poseidon::OptionalMap params);
};

}
}

#endif
