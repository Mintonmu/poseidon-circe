// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_GATE_FOYER_HPP_
#define CIRCE_PROTOCOL_GATE_FOYER_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {

#define MESSAGE_NAME   GF_UserHttpGetRequest
#define MESSAGE_ID     1001
#define MESSAGE_FIELDS \
	FIELD_STRING       (uri)	\
	FIELD_ARRAY        (headers,	\
		FIELD_STRING       (key)	\
		FIELD_STRING       (value)	\
	)	\
	FIELD_ARRAY        (params,	\
		FIELD_STRING       (key)	\
		FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   GF_UserHttpPostRequest
#define MESSAGE_ID     1002
#define MESSAGE_FIELDS \
	FIELD_STRING       (uri)	\
	FIELD_ARRAY        (headers,	\
		FIELD_STRING       (key)	\
		FIELD_STRING       (value)	\
	)	\
	FIELD_BLOB         (entity)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   GF_UserWebSocketEstablishment
#define MESSAGE_ID     1003
#define MESSAGE_FIELDS \
	FIELD_STRING       (uri)	\
	FIELD_ARRAY        (headers,	\
		FIELD_STRING       (key)	\
		FIELD_STRING       (value)	\
	)	\
	FIELD_ARRAY        (params,	\
		FIELD_STRING       (key)	\
		FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.hpp>



}
}

#endif
