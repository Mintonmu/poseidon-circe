// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass.hpp"
#include "singletons/compass_repository.hpp"
#include "orm.hpp"

namespace Circe {
namespace Pilot {

Compass::Compass(const CompassKey &compass_key)
	: m_compass_key(compass_key)
	, m_dao(Poseidon::MySql::begin_synchronization(boost::make_shared<ORM_Compass>(compass_key.to_string(), std::string(), 0, 0), false)) // Don't have it persist unless modified at all.
{
	LOG_CIRCE_DEBUG("Compass constructor (to create): compass_key = ", get_compass_key());
}
Compass::Compass(const boost::shared_ptr<ORM_Compass> &dao)
	: m_compass_key(CompassKey::from_string(dao->compass_key))
	, m_dao(Poseidon::MySql::begin_synchronization(dao, false))
{
	LOG_CIRCE_DEBUG("Compass constructor (to open): compass_key = ", get_compass_key());
}
Compass::~Compass(){
	LOG_CIRCE_DEBUG("Compass destructor: compass_key = ", get_compass_key());
}

const std::string &Compass::get_value() const {
	return m_dao->value;
}
boost::uint32_t Compass::get_version() const {
	return static_cast<boost::uint32_t>(m_dao->version);
}
boost::uint64_t Compass::get_last_access_time() const {
	return m_dao->last_access_time;
}

void Compass::touch_value(){
	const AUTO(utc_now, Poseidon::get_utc_time());
	m_dao->last_access_time = utc_now;
	CompassRepository::update_compass_indices(this);
}
void Compass::set_value(std::string value_new){
	const AUTO(version_new, m_dao->version + 1);
	const AUTO(utc_now, Poseidon::get_utc_time());
	m_dao->value = STD_MOVE(value_new);
	m_dao->version = version_new;
	m_dao->last_access_time = utc_now;
	CompassRepository::update_compass_indices(this);
}

bool Compass::is_locked_shared() const {
	return m_lock.is_locked_shared();
}
bool Compass::is_locked_shared_by(const Poseidon::Uuid &connection_uuid) const {
	return m_lock.is_locked_shared_by(connection_uuid);
}
bool Compass::try_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection) const {
	return m_lock.try_lock_shared(connection);
}
void Compass::release_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection) const {
	return m_lock.release_lock_shared(connection);
}

bool Compass::is_locked_exclusive() const {
	return m_lock.is_locked_exclusive();
}
bool Compass::is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid) const {
	return m_lock.is_locked_exclusive_by(connection_uuid);
}
bool Compass::try_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection){
	return m_lock.try_lock_exclusive(connection);
}
void Compass::release_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection){
	return m_lock.release_lock_exclusive(connection);
}

}
}
