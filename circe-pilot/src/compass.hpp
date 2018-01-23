// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_HPP_
#define CIRCE_PILOT_COMPASS_HPP_

#include <poseidon/virtual_shared_from_this.hpp>
#include <string>
#include <boost/cstdint.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/optional.hpp>
#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

class ORM_Compass;

class Compass : public virtual Poseidon::VirtualSharedFromThis {
private:
	const boost::shared_ptr<ORM_Compass> m_dop;

	boost::container::flat_map<boost::weak_ptr<Common::InterserverConnection>, boost::uint64_t> m_readers;
	boost::optional<std::pair<boost::weak_ptr<Common::InterserverConnection>, boost::uint64_t> > m_writer;

public:
	explicit Compass(const std::string &key);
	~Compass() OVERRIDE;

public:
	const std::string &get_key() const;

	const std::string &get_value() const;
	boost::uint32_t get_version() const;
	boost::uint64_t get_last_access_time() const;
	void touch_value();
	void set_value(std::string value_new);

	
};

}
}

#endif
