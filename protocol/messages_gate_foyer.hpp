// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_GATE_FOYER_HPP_
#define CIRCE_PROTOCOL_GATE_FOYER_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {

#define MESSAGE_NAME   GF_ProcessHttpRequest
#define MESSAGE_ID     1201
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (auth_token)	\
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
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   FG_ReturnHttpResponse
#define MESSAGE_ID     1202
#define MESSAGE_FIELDS \
	FIELD_VUINT        (status_code)	\
	FIELD_ARRAY        (headers,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   GF_EstablishWebSocketConnection
#define MESSAGE_ID     1203
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (auth_token)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_ARRAY        (params,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   FG_ReturnWebSocketEstablishmentResult
#define MESSAGE_ID     1204
#define MESSAGE_FIELDS \
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (message)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   GF_NotifyWebSocketClosure
#define MESSAGE_ID     1205
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (message)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   FG_KillWebSocketConnection
#define MESSAGE_ID     1206
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (message)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}

#endif
