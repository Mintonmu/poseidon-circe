// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_GATE_HPP_
#define CIRCE_PROTOCOL_GATE_HPP_

#include "messages_common.hpp"
#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Gate {

#define MESSAGE_NAME   Websocket_kill_notification
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_request
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_message_response
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Websocket_packed_broadcast_notification
#define MESSAGE_ID     1105
#define MESSAGE_FIELDS \
	FIELD_LIST         (clients,	\
	  FIELD_FIXED        (client_uuid, 16)	\
	)	\
	FIELD_REPEATED     (messages, Common_websocket_frame)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Unban_ip_request
#define MESSAGE_ID     1106
#define MESSAGE_FIELDS \
	FIELD_STRING       (client_ip)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Unban_ip_response
#define MESSAGE_ID     1107
#define MESSAGE_FIELDS \
	FIELD_VUINT        (found)	\
	//
#include <poseidon/cbpp/message_generator.inl>

}
}
}

#endif
