// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_HPP_
#define CIRCE_PILOT_COMPASS_HPP_

#include <poseidon/virtual_shared_from_this.hpp>
#include <boost/cstdint.hpp>
#include <string>
#include "compass_key.hpp"
#include "common/fwd.hpp"

namespace Circe {
namespace Pilot {

class ORM_Compass;

class Compass : public virtual Poseidon::VirtualSharedFromThis {
private:
	const CompassKey m_compass_key;
	const boost::shared_ptr<ORM_Compass> m_dao;

public:
	explicit Compass(const CompassKey &compass_key);
	explicit Compass(const boost::shared_ptr<ORM_Compass> &dao);
	~Compass() OVERRIDE;

public:
	const CompassKey &get_compass_key() const {
		return m_compass_key;
	}

	const std::string &get_value() const;
	boost::uint32_t get_version() const;
	boost::uint64_t get_last_access_time() const;

	void touch_value();
	void set_value(std::string value_new);
};

}
}

#endif
