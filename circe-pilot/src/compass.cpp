// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass.hpp"
#include "orm.hpp"

namespace Circe {
namespace Pilot {

namespace {
	boost::shared_ptr<ORM_Compass> create_dao(const CompassKey &compass_key){
		char key_str[40];
		compass_key.to_string(key_str);
		AUTO(dao, boost::make_shared<ORM_Compass>(std::string(key_str, sizeof(key_str)), std::string(), 0, 0));
		// A compass is not written into the database unless it is modified at all.
		dao->enable_auto_saving();
		return dao;
	}
}

Compass::Compass(const CompassKey &compass_key)
	: m_compass_key(compass_key), m_dao(create_dao(compass_key))
{
	LOG_CIRCE_DEBUG("Compass constructor: compass_key = ", m_compass_key);
}
Compass::~Compass(){
	LOG_CIRCE_DEBUG("Compass destructor: compass_key = ", m_compass_key);
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
}
void Compass::set_value(std::string value_new){
	const AUTO(version_new, m_dao->version + 1);
	const AUTO(utc_now, Poseidon::get_utc_time());
	m_dao->value = STD_MOVE(value_new);
	m_dao->version = version_new;
	m_dao->last_access_time = utc_now;
}

}
}
