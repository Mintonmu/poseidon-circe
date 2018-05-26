// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_BOX_HPP_
#define CIRCE_PROTOCOL_BOX_HPP_

#include "messages_common.hpp"
#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Box {

#define MESSAGE_NAME   Http_request
#define MESSAGE_ID     1901
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (auth_token)	\
	FIELD_VUINT        (verb)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_REPEATED     (params, Common_key_value)	\
	FIELD_REPEATED     (headers, Common_key_value)	\
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Http_response
#define MESSAGE_ID     1902
#define MESSAGE_FIELDS \
	FIELD_VUINT        (status_code)	\
	FIELD_REPEATED     (headers, Common_key_value)	\
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_establishment_request
#define MESSAGE_ID     1903
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (auth_token)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_REPEATED     (params, Common_key_value)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_establishment_response
#define MESSAGE_ID     1904
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_closure_notification
#define MESSAGE_ID     1905
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_request
#define MESSAGE_ID     1906
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_response
#define MESSAGE_ID     1907
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

}
}
}

#endif
