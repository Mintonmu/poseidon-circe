// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_ORM_HPP_
#define CIRCE_PILOT_ORM_HPP_

#include <poseidon/mysql/object_base.hpp>

namespace Circe {
namespace Pilot {

#define OBJECT_NAME    ORM_Compass
#define OBJECT_TABLE   "Pilot_Compass"
#define OBJECT_FIELDS  \
	FIELD_STRING       (key)	\
	FIELD_STRING       (value)	\
	FIELD_UNSIGNED     (version)	\
	FIELD_DATETIME     (last_access_time)	\
	//
#include <poseidon/mysql/object_generator.hpp>

}
}

#endif
