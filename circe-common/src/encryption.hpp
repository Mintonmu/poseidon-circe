#ifndef CIRCE_COMMON_ENCRYPTION_HPP_
#define CIRCE_COMMON_ENCRYPTION_HPP_

#include <poseidon/stream_buffer.hpp>
#include <boost/cstdint.hpp>
#include <boost/array.hpp>
#include <string>

namespace Circe {
namespace Common {

extern Poseidon::StreamBuffer encrypt(Poseidon::StreamBuffer plaintext);
extern Poseidon::StreamBuffer decrypt(Poseidon::StreamBuffer ciphertext);

}
}

#endif
