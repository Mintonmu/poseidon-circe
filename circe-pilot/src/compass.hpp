// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_HPP_
#define CIRCE_PILOT_COMPASS_HPP_

#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/uuid.hpp>
#include "compass_key.hpp"
#include "compass_lock.hpp"
#include "compass_watcher_map.hpp"

namespace Circe {
namespace Pilot {

class ORM_Compass;

class Compass : NONCOPYABLE, public virtual Poseidon::VirtualSharedFromThis {
private:
	const CompassKey m_compass_key;

	boost::shared_ptr<ORM_Compass> m_dao;
	CompassLock m_lock;
	CompassWatcherMap m_watcher_map;

public:
	explicit Compass(const CompassKey &compass_key);
	explicit Compass(const boost::shared_ptr<ORM_Compass> &dao);
	~Compass() OVERRIDE;

public:
	const CompassKey &get_compass_key() const {
		return m_compass_key;
	}

	// Data observrs and modifiers.
	boost::uint64_t get_last_access_time() const;
	void update_last_access_time();

	const std::string &get_value() const;
	boost::uint32_t get_version() const;
	void set_value(std::string value_new);

	// Interserver lock observers and modifiers.
	bool is_locked_shared() const;
	bool is_locked_shared_by(const Poseidon::Uuid &connection_uuid);
	bool try_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool release_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection);

	bool is_locked_exclusive() const;
	bool is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid);
	bool try_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool release_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection);

	// Watcher observers and modifiers.
	std::size_t get_watchers(boost::container::vector<std::pair<Poseidon::Uuid, boost::shared_ptr<Common::InterserverConnection> > > &watchers_ret) const;
	Poseidon::Uuid add_watcher(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool remove_watcher(const Poseidon::Uuid &watcher_uuid);
};

}
}

#endif
