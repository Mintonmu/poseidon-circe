// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_ERROR_CODES_HPP_
#define CIRCE_PROTOCOL_ERROR_CODES_HPP_

#include <poseidon/cbpp/status_codes.hpp>

namespace Circe {
namespace Protocol {

typedef Poseidon::Cbpp::Status_code Error_code;

namespace Error_codes {
	enum {
		error_success                              = Poseidon::Cbpp::status_ok,                    //   0
		error_internal_error                       = Poseidon::Cbpp::status_internal_error,        //  -1
		error_end_of_stream                        = Poseidon::Cbpp::status_end_of_stream,         //  -2
		error_not_found                            = Poseidon::Cbpp::status_not_found,             //  -3
		error_request_too_large                    = Poseidon::Cbpp::status_request_too_large,     //  -4
		error_bad_request                          = Poseidon::Cbpp::status_bad_request,           //  -5
		error_junk_after_packet                    = Poseidon::Cbpp::status_junk_after_packet,     //  -6
		error_forbidden                            = Poseidon::Cbpp::status_forbidden,             //  -7
		error_authorization_failure                = Poseidon::Cbpp::status_authorization_failure, //  -8
		error_length_error                         = Poseidon::Cbpp::status_length_error,          //  -9
		error_unknown_control_code                 = Poseidon::Cbpp::status_unknown_control_code,  // -10
		error_data_corrupted                       = Poseidon::Cbpp::status_data_corrupted,        // -11
		error_gone_away                            = Poseidon::Cbpp::status_gone_away,             // -12
		error_invalid_argument                     = Poseidon::Cbpp::status_invalid_argument,      // -13
		error_unsupported                          = Poseidon::Cbpp::status_unsupported,           // -14
		error_reserved_response_uninitialized      = -900,
		error_reserved_response_destroyed          = -901,

		error_gate_connection_lost                 = 50001,
		error_auth_connection_lost                 = 50002,
		error_foyer_connection_lost                = 50003,
		error_box_connection_lost                  = 50004,
		error_websocket_shadow_session_not_found   = 50005,
	};
}

using namespace Error_codes;

}
}

#endif
