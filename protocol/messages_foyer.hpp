// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_FOYER_HPP_
#define CIRCE_PROTOCOL_FOYER_HPP_

#include "messages_common.hpp"
#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Foyer {

#define MESSAGE_NAME   Http_request_to_box
#define MESSAGE_ID     1201
#define MESSAGE_FIELDS \
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

#define MESSAGE_NAME   Http_response_from_box
#define MESSAGE_ID     1202
#define MESSAGE_FIELDS \
	FIELD_FIXED        (box_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_REPEATED     (headers, Common_key_value)	\
	FIELD_FLEXIBLE     (entity)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_establishment_request_to_box
#define MESSAGE_ID     1203
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_STRING       (client_ip)	\
	FIELD_STRING       (auth_token)	\
	FIELD_STRING       (decoded_uri)	\
	FIELD_REPEATED     (params, Common_key_value)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_establishment_response_from_box
#define MESSAGE_ID     1204
#define MESSAGE_FIELDS \
	FIELD_FIXED        (box_uuid, 16)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_closure_notification_to_box
#define MESSAGE_ID     1205
#define MESSAGE_FIELDS \
	FIELD_FIXED        (box_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_kill_notification_to_gate
#define MESSAGE_ID     1206
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_request_to_box
#define MESSAGE_ID     1208
#define MESSAGE_FIELDS \
	FIELD_FIXED        (box_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_response_from_box
#define MESSAGE_ID     1209
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_request_to_gate
#define MESSAGE_ID     1210
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_response_from_gate
#define MESSAGE_ID     1211
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Check_gate_request
#define MESSAGE_ID     1212
#define MESSAGE_FIELDS \
	FIELD_FIXED        (gate_uuid, 16)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Check_gate_response
#define MESSAGE_ID     1213
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_broadcast_notification_to_gate
#define MESSAGE_ID     1214
#define MESSAGE_FIELDS \
	FIELD_LIST         (clients,	\
	  FIELD_FIXED        (gate_uuid, 16)	\
	  FIELD_FIXED        (client_uuid, 16)	\
	)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

}
}
}

#endif
