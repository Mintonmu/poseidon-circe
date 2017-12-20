// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_servlet_container.hpp"
#include "interserver_connection.hpp"

namespace Circe {
namespace Common {

boost::shared_ptr<const InterserverServletCallback> InterserverServletContainer::get_servlet(boost::uint16_t message_id) const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	const AUTO(it, m_servlets.find(message_id));
	if(it == m_servlets.end()){
		return VAL_INIT;
	}
	return it->second.lock();
}
void InterserverServletContainer::insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<const InterserverServletCallback> &servlet){
	PROFILE_ME;
	DEBUG_THROW_UNLESS(InterserverConnection::is_message_id_valid(message_id), Poseidon::Exception, Poseidon::sslit("message_id out of range"));
	DEBUG_THROW_UNLESS(servlet, Poseidon::Exception, Poseidon::sslit("Null servlet pointer"));

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	bool erase_it;
	for(AUTO(it, m_servlets.begin()); it != m_servlets.end(); erase_it ? (it = m_servlets.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
	DEBUG_THROW_UNLESS(m_servlets.emplace(message_id, servlet).second, Poseidon::Exception, Poseidon::sslit("Another servlet for this message_id already exists"));
}
bool InterserverServletContainer::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	return m_servlets.erase(message_id);
}

}
}
