// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_FOYER_SINGLETONS_SERVLET_CONTAINER_HPP_
#define CIRCE_FOYER_SINGLETONS_SERVLET_CONTAINER_HPP_

#include "common/interserver_servlet_container.hpp"

namespace Circe {
namespace Foyer {

class ServletContainer {
private:
	ServletContainer();

public:
	static boost::shared_ptr<const Common::InterserverServletContainer> get_container();
	static boost::shared_ptr<const Common::InterserverServletContainer> get_safe_container();

	static void insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<Common::InterserverServletCallback> &servlet);
	static bool remove_servlet(boost::uint16_t message_id) NOEXCEPT;
};

}
}

#endif
