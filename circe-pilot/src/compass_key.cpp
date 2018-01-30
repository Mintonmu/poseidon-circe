// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_key.hpp"
#include <poseidon/sha256.hpp>

namespace Circe {
namespace Pilot {

// Algorithm specification:
//   https://en.wikipedia.org/wiki/Ascii85
// Character table specification:
//   https://tools.ietf.org/html/rfc1924

namespace {
	CONSTEXPR const char s_table[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";

	inline bool is_character_invalid(int ch){
		return std::strchr(s_table, ch) == NULLPTR;
	}
}

CompassKey CompassKey::from_string(const std::string &str){
	PROFILE_ME;

	boost::array<char, 40> chars;
	DEBUG_THROW_UNLESS(str.copy(chars.data(), chars.size()) == chars.size(), Poseidon::Exception, Poseidon::sslit("Unexpected length of CompassKey string"));
	DEBUG_THROW_UNLESS(std::find_if(chars.begin(), chars.end(), &is_character_invalid) == chars.end(), Poseidon::Exception, Poseidon::sslit("Invalid character in CompassKey"));
	return CompassKey(chars);
}
CompassKey CompassKey::from_hash_of(const void *data, std::size_t size){
	PROFILE_ME;

	Poseidon::Sha256_ostream sha256_os;
	sha256_os.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
	const AUTO(sha256, sha256_os.finalize());
	boost::array<char, 40> chars;
	for(unsigned i = 0; i < 8; ++i){
		boost::uint32_t word = 0;
		for(unsigned j = 0; j < 4; ++j){
			word *= 256;
			word += sha256[i * 4 + j];
		}
		for(unsigned j = 0; j < 5; ++j){
			chars[i * 5 + (4 - j)] = static_cast<char>(s_table[word % 85]);
			word /= 85;
		}
	}
	return CompassKey(chars);
}

CompassKey::CompassKey(const boost::array<char, 40> &chars)
	: m_chars(chars)
{
	//
}

std::ostream &operator<<(std::ostream &os, const CompassKey &rhs){
	char str[41];
	std::copy(rhs.begin(), rhs.end(), str)[0] = 0;
	return os <<str;
}

}
}
