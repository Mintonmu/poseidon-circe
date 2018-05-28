// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_COMMON_HPP_
#define CIRCE_PROTOCOL_COMMON_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {

#define MESSAGE_NAME   Common_key_value
#define MESSAGE_ID     0
#define MESSAGE_FIELDS \
	FIELD_STRING       (key)	\
	FIELD_STRING       (value)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Common_websocket_frame
#define MESSAGE_ID     0
#define MESSAGE_FIELDS \
	FIELD_VUINT        (opcode)	\
	FIELD_FLEXIBLE     (payload)	\
	//
#include <poseidon/cbpp/message_generator.inl>

}
}

#endif
