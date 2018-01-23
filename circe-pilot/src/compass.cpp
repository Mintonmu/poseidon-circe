// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass.hpp"
#include "orm.hpp"

namespace Circe {
namespace Pilot {

namespace {
	boost::shared_ptr<ORM_Compass> create_dop(const std::string &key){
		const AUTO(utc_now, Poseidon::get_utc_time());
		AUTO(dop, boost::make_shared<ORM_Compass>(key, std::string(), 0, utc_now));
		dop->enable_auto_saving();
		return dop;
	}
}

Compass::Compass(const std::string &key)
	: m_dop(create_dop(key))
{ }
Compass::~Compass(){ }

const std::string &Compass::get_key() const {
	return m_dop->key.unlocked_get();
}

const std::string &Compass::get_value() const {
	return m_dop->value.unlocked_get();
}
boost::uint32_t Compass::get_version() const {
	return static_cast<boost::uint32_t>(m_dop->version.unlocked_get());
}
boost::uint64_t Compass::get_last_access_time() const {
	return m_dop->last_access_time.unlocked_get();
}
void Compass::touch_value(){
	m_dop->last_access_time.set(Poseidon::get_utc_time());
}
void Compass::set_value(std::string value_new){
	const AUTO(version_new, m_dop->version.unlocked_get() + 1);
	const AUTO(utc_now, Poseidon::get_utc_time());
	m_dop->value.set(STD_MOVE(value_new));
	m_dop->version.set(version_new);
	m_dop->last_access_time.set(utc_now);
}

}
}
