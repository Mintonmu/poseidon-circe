// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "cbpp_servlet_map.hpp"

namespace Circe {
namespace Common {

CbppServletMap::CbppServletMap(){ }
CbppServletMap::~CbppServletMap(){ }

const CbppServletMap::ServletCallback *CbppServletMap::get(boost::uint64_t message_id) const {
	PROFILE_ME;

	const AUTO(it, m_servlets.find(message_id));
	if(it == m_servlets.end()){
		LOG_CIRCE_DEBUG("Servlet not found: message_id = ", message_id);
		return NULLPTR;
	}
	return &(it->second);
}
void CbppServletMap::insert(boost::uint64_t message_id, ServletCallback callback){
	PROFILE_ME;

	const AUTO(pair, m_servlets.emplace(message_id, STD_MOVE_IDN(callback)));
	if(!pair.second){
		LOG_CIRCE_WARNING("Duplicate servlet: message_id = ", message_id);
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Duplicate servlet"));
	}
}
bool CbppServletMap::remove(boost::uint64_t message_id) NOEXCEPT {
	PROFILE_ME;

	const AUTO(it, m_servlets.find(message_id));
	if(it == m_servlets.end()){
		LOG_CIRCE_DEBUG("Servlet not found: message_id = ", message_id);
		return false;
	}
	m_servlets.erase(it);
	return true;
}
void CbppServletMap::clear() NOEXCEPT {
	PROFILE_ME;

	m_servlets.clear();
}

}
}
