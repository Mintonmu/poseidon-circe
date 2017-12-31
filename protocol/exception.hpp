// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_EXCEPTION_HPP_
#define CIRCE_PROTOCOL_EXCEPTION_HPP_

#include <poseidon/exception.hpp>
#include <poseidon/cbpp/exception.hpp>
#include "error_codes.hpp"

namespace Circe {
namespace Protocol {

typedef Poseidon::Cbpp::Exception Exception;

}
}

#define CIRCE_PROTOCOL_THROW_UNLESS(cond_, ...)   DEBUG_THROW_UNLESS(cond_, ::Circe::Protocol::Exception, __VA_ARGS__)
#define CIRCE_PROTOCOL_THROW(...)                 DEBUG_THROW(::Circe::Protocol::Exception, __VA_ARGS__)

#endif
