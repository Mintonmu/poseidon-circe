// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_watcher_map.hpp"
#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

Compass_watcher_map::Compass_watcher_map(){
	//
}
Compass_watcher_map::~Compass_watcher_map(){
	//
}

std::size_t Compass_watcher_map::get_watchers(boost::container::vector<std::pair<Poseidon::Uuid, boost::shared_ptr<Common::Interserver_connection> > > &watchers_ret) const {
	PROFILE_ME;

	std::size_t count_collected = 0;
	watchers_ret.reserve(watchers_ret.size() + m_watchers.size());
	for(AUTO(it, m_watchers.begin()); it != m_watchers.end(); ++it){
		AUTO(connection, it->second.lock());
		if(!connection){
			continue;
		}
		const AUTO_REF(watcher_uuid, it->first);
		watchers_ret.emplace_back(watcher_uuid, STD_MOVE(connection));
		++count_collected;
	}
	return count_collected;
}
Poseidon::Uuid Compass_watcher_map::add_watcher(const boost::shared_ptr<Common::Interserver_connection> &connection){
	PROFILE_ME;

	bool erase_it;
	for(AUTO(it, m_watchers.begin()); it != m_watchers.end(); erase_it ? (it = m_watchers.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
	const AUTO(watcher_uuid, Poseidon::Uuid::random());
	const AUTO(pair, m_watchers.emplace(watcher_uuid, connection));
	DEBUG_THROW_ASSERT(pair.second);
	return watcher_uuid;
}
bool Compass_watcher_map::remove_watcher(const Poseidon::Uuid &watcher_uuid) NOEXCEPT {
	PROFILE_ME;

	const AUTO(it, m_watchers.find(watcher_uuid));
	if(it == m_watchers.end()){
		return false;
	}
	m_watchers.erase(it);
	return true;
}

}
}
