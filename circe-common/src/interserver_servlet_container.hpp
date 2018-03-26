// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_SERVLET_CONTAINER_HPP_
#define CIRCE_COMMON_INTERSERVER_SERVLET_CONTAINER_HPP_

#include <poseidon/fwd.hpp>
#include <boost/container/flat_map.hpp>
#include "interserver_servlet_callback.hpp"

namespace Circe {
namespace Common {

class Interserver_servlet_container {
private:
	boost::container::flat_map<boost::uint16_t, boost::weak_ptr<const Interserver_servlet_callback> > m_servlets;

public:
	boost::shared_ptr<const Interserver_servlet_callback> get_servlet(boost::uint16_t message_id) const;
	void insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<const Interserver_servlet_callback> &callback);
	bool remove_servlet(boost::uint16_t message_id) NOEXCEPT;
};

}
}

#endif
