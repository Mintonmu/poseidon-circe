// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "servlet_container.hpp"

namespace Circe {
namespace Foyer {

namespace {
	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Common::InterserverServletContainer> g_weak_container;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	PROFILE_ME;

	const AUTO(weak_container, boost::make_shared<Common::InterserverServletContainer>());
	handles.push(weak_container);
	g_weak_container = weak_container;
}

boost::shared_ptr<const Common::InterserverServletContainer> ServletContainer::get_container(){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	AUTO(container, g_weak_container.lock());
	if(!container){
		return VAL_INIT;
	}
	return STD_MOVE_IDN(container);
}
boost::shared_ptr<const Common::InterserverServletContainer> ServletContainer::get_safe_container(){
	PROFILE_ME;

	AUTO(container, get_container());
	DEBUG_THROW_ASSERT(container);
	return container;
}

void ServletContainer::insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<Common::InterserverServletCallback> &servlet){
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(container, g_weak_container.lock());
	if(!container){
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Another servlet with the same message_id already exists"));
	}
	container->insert_servlet(message_id, servlet);
}
bool ServletContainer::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(g_mutex);
	const AUTO(container, g_weak_container.lock());
	if(!container){
		return false;
	}
	return container->remove_servlet(message_id);
}

}
}
