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
public:
	static CompassKey from_string(const std::string &str);
	static CompassKey from_hash_of(const void *data, std::size_t size);

private:
	boost::array<char, 40> m_chars;

private:
	explicit CompassKey(const boost::array<char, 40> &chars);

public:
	const char *begin() const {
		return m_chars.begin();
	}
	const char *end() const {
		return m_chars.end();
	}
	const char *data() const {
		return m_chars.data();
	}
	std::size_t size() const {
		return m_chars.size();
	}

	std::string to_string() const {
		return std::string(m_chars.begin(), m_chars.end());
	}
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
