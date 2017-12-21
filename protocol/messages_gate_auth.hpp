// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_GATE_AUTH_HPP_
#define CIRCE_PROTOCOL_GATE_AUTH_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {

#define MESSAGE_NAME   GA_ClientHttpAuthenticationRequest
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_FIXED        (session_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_VUINT        (verb)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_ARRAY        (params,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	FIELD_ARRAY        (headers,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   AG_ClientHttpAuthenticationResponse
#define MESSAGE_ID     1102
#define MESSAGE_FIELDS \
	FIELD_VUINT        (http_status_code)	\
	FIELD_ARRAY        (headers,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	FIELD_STRING       (auth_token)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   GA_ClientWebSocketAuthenticationRequest
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_FIXED        (session_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_ARRAY        (params,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   AG_ClientWebSocketAuthenticationResponse
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	FIELD_VUINT        (websocket_status_code)	\
	FIELD_STRING       (message)	\
	FIELD_STRING       (auth_token)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}

#endif
