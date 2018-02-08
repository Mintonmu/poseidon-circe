// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_WATCHER_MAP_HPP_
#define CIRCE_PILOT_COMPASS_WATCHER_MAP_HPP_

#include <poseidon/cxx_util.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/vector.hpp>
#include "common/fwd.hpp"

namespace Circe {
namespace Pilot {

class CompassWatcherMap : NONCOPYABLE {
private:
	boost::container::flat_map<Poseidon::Uuid, boost::weak_ptr<Common::InterserverConnection> > m_watchers;

public:
	CompassWatcherMap();
	~CompassWatcherMap();

public:
	std::size_t get_watchers(boost::container::vector<std::pair<Poseidon::Uuid, boost::shared_ptr<Common::InterserverConnection> > > &watchers_ret) const;
	Poseidon::Uuid add_watcher(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool remove_watcher(const Poseidon::Uuid &watcher_uuid) NOEXCEPT;
};

}
}

#endif
