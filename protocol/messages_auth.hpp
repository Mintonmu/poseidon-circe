// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_AUTH_HPP_
#define CIRCE_PROTOCOL_AUTH_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Auth {

#define MESSAGE_NAME   HttpAuthenticationRequest
#define MESSAGE_ID     1801
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
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

#define MESSAGE_NAME   HttpAuthenticationResponse
#define MESSAGE_ID     1802
#define MESSAGE_FIELDS \
	FIELD_STRING       (auth_token)	\
	FIELD_VUINT        (status_code)	\
	FIELD_ARRAY        (headers,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   WebSocketAuthenticationRequest
#define MESSAGE_ID     1803
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_ARRAY        (params,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   WebSocketAuthenticationResponse
#define MESSAGE_ID     1804
#define MESSAGE_FIELDS \
	FIELD_LIST         (messages,	\
		FIELD_VUINT        (opcode)	\
		FIELD_FLEXIBLE     (payload)	\
	)	\
	FIELD_STRING       (auth_token)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}
}

#endif
