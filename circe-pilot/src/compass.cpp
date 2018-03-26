// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass.hpp"
#include "singletons/compass_repository.hpp"
#include "orm.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_pilot.hpp"
#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

Compass::Compass(const Compass_key &compass_key)
	: m_compass_key(compass_key)
{
	LOG_CIRCE_DEBUG("Compass constructor (to create): compass_key = ", get_compass_key());
}
Compass::Compass(const boost::shared_ptr<ORM_Compass> &dao)
	: m_compass_key(Compass_key::from_string(dao->compass_key))
	, m_dao(Poseidon::Mysql::begin_synchronization(dao, false)), m_lock(), m_watcher_map()
{
	LOG_CIRCE_DEBUG("Compass constructor (to open): compass_key = ", get_compass_key());
}
Compass::~Compass(){
	LOG_CIRCE_DEBUG("Compass destructor: compass_key = ", get_compass_key());
}

boost::uint64_t Compass::get_last_access_time() const {
	if(!m_dao){
		return 0;
	}
	return m_dao->last_access_time;
}
void Compass::update_last_access_time(){
	if(!m_dao){
		return;
	}
	const AUTO(utc_now, Poseidon::get_utc_time());
	m_dao->last_access_time = utc_now;
	Compass_repository::update_compass_indices(this);
}

const std::string &Compass::get_value() const {
	if(!m_dao){
		return Poseidon::empty_string();
	}
	return m_dao->value;
}
boost::uint64_t Compass::get_version() const {
	if(!m_dao){
		return 0;
	}
	return m_dao->version;
}
void Compass::set_value(std::string value_new){
	// Mandate exclusive access.
	DEBUG_THROW_UNLESS(m_lock.is_locked_exclusive(), Poseidon::Exception, Poseidon::sslit("Exclusive access is required to alter the value of a compass"));
	// Collect watchers before modifying the value.
	boost::container::vector<std::pair<Poseidon::Uuid, boost::shared_ptr<Common::Interserver_connection> > > watchers;
	m_watcher_map.get_watchers(watchers);

	// Modify the value now.
	if(!m_dao){
		m_dao = Poseidon::Mysql::begin_synchronization(boost::make_shared<ORM_Compass>(m_compass_key.to_string(), 0, std::string(), 0), false);
	}
	m_dao->value = STD_MOVE(value_new);
	m_dao->version = m_dao->version + 1;

	// Notify all watchers that the value has been changed.
	for(AUTO(it, watchers.begin()); it != watchers.end(); ++it){
		const AUTO_REF(watcher_uuid, it->first);
		const AUTO_REF(connection, it->second);
		try {
			Protocol::Pilot::Value_change_notification ntfy;
			ntfy.watcher_uuid = watcher_uuid;
			ntfy.value_new    = m_dao->value;
			connection->send_notification(ntfy);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			connection->shutdown(Protocol::error_internal_error, e.what());
		}
	}
}

bool Compass::is_locked_shared() const {
	return m_lock.is_locked_shared();
}
bool Compass::is_locked_shared_by(const Poseidon::Uuid &connection_uuid){
	return m_lock.is_locked_shared_by(connection_uuid);
}
bool Compass::try_lock_shared(const boost::shared_ptr<Common::Interserver_connection> &connection){
	return m_lock.try_lock_shared(connection);
}
bool Compass::release_lock_shared(const boost::shared_ptr<Common::Interserver_connection> &connection){
	return m_lock.release_lock_shared(connection);
}

bool Compass::is_locked_exclusive() const {
	return m_lock.is_locked_exclusive();
}
bool Compass::is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid){
	return m_lock.is_locked_exclusive_by(connection_uuid);
}
bool Compass::try_lock_exclusive(const boost::shared_ptr<Common::Interserver_connection> &connection){
	return m_lock.try_lock_exclusive(connection);
}
bool Compass::release_lock_exclusive(const boost::shared_ptr<Common::Interserver_connection> &connection){
	return m_lock.release_lock_exclusive(connection);
}

std::size_t Compass::get_watchers(boost::container::vector<std::pair<Poseidon::Uuid, boost::shared_ptr<Common::Interserver_connection> > > &watchers_ret) const {
	return m_watcher_map.get_watchers(watchers_ret);
}
Poseidon::Uuid Compass::add_watcher(const boost::shared_ptr<Common::Interserver_connection> &connection){
	return m_watcher_map.add_watcher(connection);
}
bool Compass::remove_watcher(const Poseidon::Uuid &watcher_uuid){
	return m_watcher_map.remove_watcher(watcher_uuid);
}

}
}
