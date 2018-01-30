// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_PILOT_HPP_
#define CIRCE_PROTOCOL_PILOT_HPP_

#include <poseidon/cbpp/message_base.hpp>

namespace Circe {
namespace Protocol {
namespace Pilot {

// lock_disposition
enum {
	LDP_LEAVE_ALONE            =  0, // Do not try acquiring a lock.
	LDP_TRY_ACQUIRE_EXCLUSIVE  = 21, // Try acquiring the lock for exclusive (write) access. The lock is recursive.
	LDP_TRY_ACQUIRE_SHARED     = 22, // Try acquiring the lock for shared (read-only) access. The lock is recursive.
	LDP_RELEASE_EXCLUSIVE      = 28, // Negate a previous `LDP_TRY_ACQUIRE_EXCLUSIVE` operation.
	LDP_RELEASE_SHARED         = 29, // Negate a previous `LDP_TRY_ACQUIRE_SHARED` operation.
};

// lock_state_{old,new}
enum {
	LST_FREE                   = 91, // The lock is not locked.
	LST_LOCKED_EXCLUSIVE       = 92, // The lock is in exclusive mode.
	LST_LOCKED_SHARED          = 93, // The lock is in shared mode.
};

#define MESSAGE_NAME   CompareExchangeRequest
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_STRING       (key)	\
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
	FIELD_VUINT        (successful)	\
	FIELD_VUINT        (criterion_index)	\
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (lock_state_old)	\
	FIELD_VUINT        (lock_state_new)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   ExchangeRequest
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_STRING       (key)	\
	FIELD_STRING       (value_new)	\
	FIELD_VUINT        (lock_disposition)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

#define MESSAGE_NAME   ExchangeResponse
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	FIELD_VUINT        (successful)	\
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (lock_state_old)	\
	FIELD_VUINT        (lock_state_new)	\
	//
#include <poseidon/cbpp/message_generator.hpp>

}
}
}

#endif
