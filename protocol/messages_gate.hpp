// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_GATE_HPP_
#define CIRCE_PROTOCOL_GATE_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Gate {

#define MESSAGE_NAME   WebSocketKillRequest
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_VUINT        (status_code)	\
	FIELD_STRING       (reason)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   WebSocketKillResponse
#define MESSAGE_ID     1102
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   WebSocketPackedMessageRequest
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_FIXED        (client_uuid, 16)	\
	FIELD_LIST         (messages,	\
	  FIELD_VUINT        (opcode)\
	  FIELD_FLEXIBLE     (payload)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   WebSocketPackedMessageResponse
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}
}

#endif
