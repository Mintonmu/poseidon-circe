// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_servlet_container.hpp"
#include "interserver_connection.hpp"

namespace Circe {
namespace Common {

boost::shared_ptr<const Interserver_servlet_callback> Interserver_servlet_container::get_servlet(boost::uint16_t message_id) const {
	POSEIDON_PROFILE_ME;

	const AUTO(it, m_servlets.find(message_id));
	if(it == m_servlets.end()){
		return VAL_INIT;
	}
	return it->second.lock();
}
void Interserver_servlet_container::insert_servlet(boost::uint16_t message_id, const boost::shared_ptr<const Interserver_servlet_callback> &servlet){
	POSEIDON_PROFILE_ME;
	POSEIDON_THROW_UNLESS(Interserver_connection::is_message_id_valid(message_id), Poseidon::Exception, Poseidon::Rcnts::view("message_id out of range"));
	POSEIDON_THROW_UNLESS(servlet, Poseidon::Exception, Poseidon::Rcnts::view("Null servlet pointer"));

	bool erase_it;
	for(AUTO(it, m_servlets.begin()); it != m_servlets.end(); erase_it ? (it = m_servlets.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
	CIRCE_LOG_DEBUG("Registering servlet: message_id = ", message_id);
	POSEIDON_THROW_UNLESS(m_servlets.emplace(message_id, servlet).second, Poseidon::Exception, Poseidon::Rcnts::view("Another servlet for this message_id already exists"));
}
bool Interserver_servlet_container::remove_servlet(boost::uint16_t message_id) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	return m_servlets.erase(message_id) != 0;
}

}
}
