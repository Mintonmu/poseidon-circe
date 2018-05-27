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

Compass_key Compass_key::from_string(const std::string &str){
	POSEIDON_PROFILE_ME;

	boost::array<char, 40> chars;
	POSEIDON_THROW_UNLESS(str.copy(chars.data(), chars.size()) == chars.size(), Poseidon::Exception, Poseidon::Rcnts::view("Unexpected length of Compass_key string"));
	POSEIDON_THROW_UNLESS(std::find_if(chars.begin(), chars.end(), &is_character_invalid) == chars.end(), Poseidon::Exception, Poseidon::Rcnts::view("Invalid character in Compass_key"));
	return Compass_key(chars);
}
Compass_key Compass_key::from_hash_of(const void *data, std::size_t size){
	POSEIDON_PROFILE_ME;

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
	return Compass_key(chars);
}

Compass_key::Compass_key(const boost::array<char, 40> &chars)
	: m_chars(chars)
{
	//
}

std::ostream &operator<<(std::ostream &os, const Compass_key &rhs){
	char str[41];
	std::copy(rhs.begin(), rhs.end(), str)[0] = 0;
	return os <<str;
}

}
}
