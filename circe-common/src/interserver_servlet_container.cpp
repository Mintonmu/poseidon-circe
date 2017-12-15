// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_servlet_container.hpp"
#include "interserver_connection.hpp"

namespace Circe {
namespace Common {

boost::shared_ptr<const InterserverServletContainer::ServletCallback> InterserverServletContainer::get_servlet(boost::uint16_t message_id) const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	const AUTO(it, m_map.find(message_id));
	if(it == m_map.end()){
		return VAL_INIT;
	}
	return it->second;
}
void InterserverServletContainer::insert_servlet(boost::uint16_t message_id, ServletCallback callback){
	PROFILE_ME;
	DEBUG_THROW_UNLESS(InterserverConnection::is_message_id_valid(message_id), Poseidon::Exception, Poseidon::sslit("message_id out of range"));

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(m_map.count(message_id) == 0, Poseidon::Exception, Poseidon::sslit("Duplicate servlet for message"));
	m_map.emplace(message_id, boost::make_shared<ServletCallback>(STD_MOVE_IDN(callback)));
}
bool InterserverServletContainer::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	return m_map.erase(message_id);
}

}
}
