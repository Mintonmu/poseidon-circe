// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_LOCK_HPP_
#define CIRCE_PILOT_COMPASS_LOCK_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/uuid.hpp>
#include <boost/container/flat_map.hpp>
#include "common/fwd.hpp"

namespace Circe {
namespace Pilot {

class CompassLock : NONCOPYABLE {
private:
	boost::container::flat_multimap<Poseidon::Uuid, boost::weak_ptr<Common::InterserverConnection> > m_readers;
	boost::container::flat_multimap<Poseidon::Uuid, boost::weak_ptr<Common::InterserverConnection> > m_writers;

public:
	CompassLock();
	~CompassLock();

private:
	void collect_expired_connections();

public:
	bool is_locked_shared();
	bool is_locked_shared_by(const Poseidon::Uuid &connection_uuid);
	bool try_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection);
	void release_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection);

	bool is_locked_exclusive();
	bool is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid);
	bool try_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection);
	void release_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection);
};

}
}

#endif
