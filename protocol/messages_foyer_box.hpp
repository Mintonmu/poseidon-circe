// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_FOYER_BOX_HPP_
#define CIRCE_PROTOCOL_FOYER_BOX_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {

#define MESSAGE_NAME   FB_ProcessHttpRequest
#define MESSAGE_ID     1901
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
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

#define MESSAGE_NAME   BF_ReturnHttpResponse
#define MESSAGE_ID     1902
#define MESSAGE_FIELDS \
	FIELD_VUINT        (status_code)	\
	FIELD_ARRAY        (headers,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   FB_EstablishWebSocketConnection
#define MESSAGE_ID     1903
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
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

#define MESSAGE_NAME   BF_ReturnWebSocketEstablishmentResult
#define MESSAGE_ID     1904
#define MESSAGE_FIELDS \
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (message)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   FB_NotifyWebSocketClosure
#define MESSAGE_ID     1905
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (message)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}

#endif
