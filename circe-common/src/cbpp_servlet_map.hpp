// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_CBPP_SERVLET_MAP_HPP_
#define CIRCE_COMMON_CBPP_SERVLET_MAP_HPP_

#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>
#include <boost/container/flat_map.hpp>
#include <poseidon/fwd.hpp>

namespace Circe {
namespace Common {

class CbppConnection;
class CbppResponse;

class CbppServletMap {
public:
	typedef boost::function<CbppResponse (const boost::shared_ptr<CbppConnection> &connection, boost::uint64_t message_id, Poseidon::StreamBuffer payload)> ServletCallback;

private:
	boost::container::flat_map<boost::uint64_t, ServletCallback> m_servlets;

public:
	CbppServletMap();
	~CbppServletMap();

public:
	const ServletCallback *get(boost::uint64_t message_id) const;
	void insert(boost::uint64_t message_id, ServletCallback callback);
	bool remove(boost::uint64_t message_id) NOEXCEPT;
	void clear() NOEXCEPT;
};

}
}

#endif
