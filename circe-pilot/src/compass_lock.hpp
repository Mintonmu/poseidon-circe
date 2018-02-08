// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PILOT_COMPASS_LOCK_HPP_
#define CIRCE_PILOT_COMPASS_LOCK_HPP_

#include <poseidon/cxx_util.hpp>
#include <poseidon/uuid.hpp>
#include <boost/container/flat_map.hpp>
#include "common/fwd.hpp"

namespace Circe {
namespace Pilot {

class CompassLock : NONCOPYABLE {
private:
	// These members have to be `mutable` because of lazy deletion.
	mutable boost::container::flat_multimap<Poseidon::Uuid, boost::weak_ptr<Common::InterserverConnection> > m_readers;
	mutable boost::container::flat_multimap<Poseidon::Uuid, boost::weak_ptr<Common::InterserverConnection> > m_writers;

public:
	CompassLock();
	~CompassLock();

public:
	void collect_expired_connections() const NOEXCEPT;

	bool is_locked_shared() const NOEXCEPT;
	bool is_locked_shared_by(const Poseidon::Uuid &connection_uuid) NOEXCEPT;
	bool try_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool release_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection) NOEXCEPT;

	bool is_locked_exclusive() const NOEXCEPT;
	bool is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid) NOEXCEPT;
	bool try_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection);
	bool release_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection) NOEXCEPT;
};

}
}

#endif
