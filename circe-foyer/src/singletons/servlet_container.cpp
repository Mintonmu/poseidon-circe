// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "servlet_container.hpp"

namespace Circe {
namespace Foyer {

namespace {
	Poseidon::Mutex g_mutex;
	Common::InterserverServletContainer g_container;
}

boost::shared_ptr<const Common::InterserverServletCallback> ServletContainer::get_servlet(boost::uint16_t message_id){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	return g_container.get_servlet(message_id);
}
void ServletContainer::insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<Common::InterserverServletCallback> &servlet){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	g_container.insert_servlet(message_id, servlet);
}
bool ServletContainer::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	return g_container.remove_servlet(message_id);
}

}
}
