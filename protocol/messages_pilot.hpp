// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_PILOT_HPP_
#define CIRCE_PROTOCOL_PILOT_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Pilot {

// Constants for `lock_disposition`.
enum {
	LOCK_LEAVE_ALONE            =  0,
	LOCK_TRY_ACQUIRE_SHARED     = 11,
	LOCK_TRY_ACQUIRE_EXCLUSIVE  = 12,
	LOCK_RELEASE_SHARED         = 31,
	LOCK_RELEASE_EXCLUSIVE      = 32,
};

#define MESSAGE_NAME   CompareExchangeRequest
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	FIELD_ARRAY        (criteria,	\
	  FIELD_STRING       (value_cmp)	\
	  FIELD_STRING       (value_new)	\
	)	\
	FIELD_VUINT        (lock_disposition)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   CompareExchangeResponse
#define MESSAGE_ID     1102
#define MESSAGE_FIELDS \
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (version_old)	\
	FIELD_VUINT        (succeeded)	\
	FIELD_VUINT        (criterion_index)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   ExchangeRequest
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	FIELD_STRING       (value_new)	\
	FIELD_VUINT        (lock_disposition)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   ExchangeResponse
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (version_old)	\
	FIELD_VUINT        (succeeded)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}
}

#endif
