// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_key.hpp"
#include <poseidon/sha256.hpp>

namespace Circe {
namespace Pilot {

CompassKey::CompassKey(const void *data, std::size_t size){
	Poseidon::Sha256_ostream sha256_os;
	sha256_os.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
	m_data = sha256_os.finalize();
}

void CompassKey::to_string(char (&str)[40]) const {
	// https://en.wikipedia.org/wiki/Ascii85
	for(unsigned i = 0; i < 8; ++i){
		boost::uint32_t word = 0;
		for(unsigned j = 0; j < 4; ++j){
			word *= 256;
			word += m_data[i * 4 + j];
		}
		for(unsigned j = 0; j < 5; ++j){
			str[i * 5 + (4 - j)] = static_cast<char>(word % 85 + 33);
			word /= 85;
		}
	}
}

std::ostream &operator<<(std::ostream &os, const CompassKey &rhs){
	char temp[41];
	rhs.to_string(reinterpret_cast<char (&)[40]>(temp));
	temp[40] = 0;
	return os <<temp;
}

}
}
