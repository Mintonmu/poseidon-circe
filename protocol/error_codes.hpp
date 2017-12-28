// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_ERROR_CODES_HPP_
#define CIRCE_PROTOCOL_ERROR_CODES_HPP_

namespace Circe {
namespace Protocol {

namespace ErrorCodes {
	enum {
		ERR_SUCCESS                           =     0,
		ERR_INTERNAL_ERROR                    =    -1,
		ERR_END_OF_STREAM                     =    -2,
		ERR_NOT_FOUND                         =    -3,
		ERR_REQUEST_TOO_LARGE                 =    -4,
		ERR_BAD_REQUEST                       =    -5,
		ERR_JUNK_AFTER_PACKET                 =    -6,
		ERR_FORBIDDEN                         =    -7,
		ERR_AUTHORIZATION_FAILURE             =    -8,
		ERR_LENGTH_ERROR                      =    -9,
		ERR_UNKNOWN_CONTROL_CODE              =   -10,
		ERR_DATA_CORRUPTED                    =   -11,
		ERR_GONE_AWAY                         =   -12,
		ERR_INVALID_ARGUMENT                  =   -13,
		ERR_UNSUPPORTED                       =   -14,

		ERR_GATE_CONNECTION_LOST              = 50001,
		ERR_AUTH_CONNECTION_LOST              = 50002,
		ERR_FOYER_CONNECTION_LOST             = 50003,
		ERR_BOX_CONNECTION_LOST               = 50004,
		ERR_CLIENT_CONNECTION_LOST            = 50005,
		ERR_CLIENT_SHADOW_SESSION_NOT_FOUND   = 50006,
	};
}
using namespace ErrorCodes;

}
}

#endif
