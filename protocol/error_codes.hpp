// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_ERROR_CODES_HPP_
#define CIRCE_PROTOCOL_ERROR_CODES_HPP_

#include <poseidon/cbpp/status_codes.hpp>

namespace Circe {
namespace Protocol {

typedef Poseidon::Cbpp::StatusCode ErrorCode;

namespace ErrorCodes {
	enum {
		ERR_SUCCESS                              = Poseidon::Cbpp::ST_OK,                    //   0
		ERR_INTERNAL_ERROR                       = Poseidon::Cbpp::ST_INTERNAL_ERROR,        //  -1
		ERR_END_OF_STREAM                        = Poseidon::Cbpp::ST_INTERNAL_ERROR,        //  -2
		ERR_NOT_FOUND                            = Poseidon::Cbpp::ST_NOT_FOUND,             //  -3
		ERR_REQUEST_TOO_LARGE                    = Poseidon::Cbpp::ST_REQUEST_TOO_LARGE,     //  -4
		ERR_BAD_REQUEST                          = Poseidon::Cbpp::ST_BAD_REQUEST,           //  -5
		ERR_JUNK_AFTER_PACKET                    = Poseidon::Cbpp::ST_JUNK_AFTER_PACKET,     //  -6
		ERR_FORBIDDEN                            = Poseidon::Cbpp::ST_FORBIDDEN,             //  -7
		ERR_AUTHORIZATION_FAILURE                = Poseidon::Cbpp::ST_AUTHORIZATION_FAILURE, //  -8
		ERR_LENGTH_ERROR                         = Poseidon::Cbpp::ST_LENGTH_ERROR,          //  -9
		ERR_UNKNOWN_CONTROL_CODE                 = Poseidon::Cbpp::ST_UNKNOWN_CONTROL_CODE,  // -10
		ERR_DATA_CORRUPTED                       = Poseidon::Cbpp::ST_DATA_CORRUPTED,        // -11
		ERR_GONE_AWAY                            = Poseidon::Cbpp::ST_GONE_AWAY,             // -12
		ERR_INVALID_ARGUMENT                     = Poseidon::Cbpp::ST_INVALID_ARGUMENT,      // -13
		ERR_UNSUPPORTED                          = Poseidon::Cbpp::ST_UNSUPPORTED,           // -14

		ERR_GATE_CONNECTION_LOST                 = 50001,
		ERR_AUTH_CONNECTION_LOST                 = 50002,
		ERR_FOYER_CONNECTION_LOST                = 50003,
		ERR_BOX_CONNECTION_LOST                  = 50004,
		ERR_WEBSOCKET_SHADOW_SESSION_NOT_FOUND   = 50005,
		ERR_RESERVED_RESPONSE_UNINITIALIZED      = 50006,
	};
}

using namespace ErrorCodes;

}
}

#endif
