// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_KEY_HPP_
#define CIRCE_PILOT_COMPASS_KEY_HPP_

#include <poseidon/fwd.hpp>
#include <boost/array.hpp>
#include <cstddef>
#include <cstring>
#include <iosfwd>

namespace Circe {
namespace Pilot {

class CompassKey {
private:
	boost::array<unsigned char, 32> m_data;

public:
	CompassKey(const void *data, std::size_t size);

public:
	const unsigned char *data() const {
		return m_data.data();
	}
	std::size_t size() const {
		return m_data.size();
	}

	void to_string(char (&str)[40]) const;
};

inline bool operator==(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}
inline bool operator!=(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) != 0;
}
inline bool operator<(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) < 0;
}
inline bool operator>(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) > 0;
}
inline bool operator<=(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) <= 0;
}
inline bool operator>=(const CompassKey &lhs, const CompassKey &rhs){
	return std::memcmp(lhs.data(), rhs.data(), lhs.size()) >= 0;
}

extern std::ostream &operator<<(std::ostream &os, const CompassKey &rhs);

}
}

#endif
