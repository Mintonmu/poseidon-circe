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
	lock_leave_alone            =  0,
	lock_try_acquire_shared     = 11,
	lock_try_acquire_exclusive  = 12,
	lock_release_shared         = 21,
	lock_release_exclusive      = 22,
};

// Constants for `lock_state`.
enum {
	lock_free_for_acquisition   =  0,
	lock_shared_by_me           = 30,
	lock_shared_by_others       = 31,
	lock_exclusive_by_me        = 40,
	lock_exclusive_by_others    = 41,
};

#define MESSAGE_NAME   Compare_exchange_request
#define MESSAGE_ID     1101
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	FIELD_ARRAY        (criteria,	\
	  FIELD_STRING       (value_cmp)	\
	  FIELD_STRING       (value_new)	\
	)	\
	FIELD_VUINT        (lock_disposition)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Compare_exchange_response
#define MESSAGE_ID     1102
#define MESSAGE_FIELDS \
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (version_old)	\
	FIELD_VUINT        (succeeded)	\
	FIELD_VUINT        (criterion_index)	\
	FIELD_VUINT        (lock_state)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Exchange_request
#define MESSAGE_ID     1103
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	FIELD_STRING       (value_new)	\
	FIELD_VUINT        (lock_disposition)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Exchange_response
#define MESSAGE_ID     1104
#define MESSAGE_FIELDS \
	FIELD_STRING       (value_old)	\
	FIELD_VUINT        (version_old)	\
	FIELD_VUINT        (succeeded)	\
	FIELD_VUINT        (lock_state)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Add_watch_request
#define MESSAGE_ID     1105
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Add_watch_response
#define MESSAGE_ID     1106
#define MESSAGE_FIELDS \
	FIELD_FIXED        (watcher_uuid, 16)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Remove_watch_notification
#define MESSAGE_ID     1107
#define MESSAGE_FIELDS \
	FIELD_BLOB         (key)	\
	FIELD_FIXED        (watcher_uuid, 16)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Value_change_notification
#define MESSAGE_ID     1108
#define MESSAGE_FIELDS \
	FIELD_FIXED        (watcher_uuid, 16)	\
	FIELD_STRING       (value_new)	\
	//
#include <poseidon/cbpp/message_generator.inl>

}
}
}

#endif
