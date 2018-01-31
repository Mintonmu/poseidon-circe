// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/servlet_container.hpp"
#include "common/interserver_connection.hpp"
#include "common/define_interserver_servlet_for.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "protocol/messages_pilot.hpp"
#include "singletons/compass_repository.hpp"

#define DEFINE_SERVLET_FOR(...)   CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(::Circe::Pilot::ServletContainer::insert_servlet, __VA_ARGS__)

namespace Circe {
namespace Pilot {

DEFINE_SERVLET_FOR(Protocol::Pilot::ExchangeRequest, connection, req){
	const AUTO(compass, CompassKeyRepository::open_compass(CompassKey::from_string(req.key)));
	DEBUG_THROW_ASSERT(compass);
	LOG_CIRCE_DEBUG("Opened compass: ", compass->get_compass_key());

	int shared_lock_disposition;
	int exclusive_lock_disposition;
	switch(req.lock_disposition){
	case Protocol::Pilot::LOCK_LEAVE_ALONE:
		m_shared_lock_disposition    = 0;
		m_exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_SHARED:
		m_shared_lock_disposition    = 1;
		m_exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_EXCLUSIVE:
		m_shared_lock_disposition    = 0;
		m_exclusive_lock_disposition = 1;
		break;
	case Protocol::Pilot::LOCK_RELEASE_SHARED:
		m_shared_lock_disposition    = -1;
		m_exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_RELEASE_EXCLUSIVE:
		m_shared_lock_disposition    = 0;
		m_exclusive_lock_disposition = -1;
		break;
	default:
		LOG_CIRCE_ERROR("Unknown lock_disposition: ", lock_disposition);
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Unknown lock_disposition"));
	}
	DEBUG_THROW_UNLESS(!(shared_lock_disposition < 0) || compass->is_locked_shared_by(connection->get_connection_uuid()));
	DEBUG_THROW_UNLESS(!(exclusive_lock_disposition < 0) || compass->is_locked_exclusive_by(connection->get_connection_uuid()));

	std::string value_old = compass->get_value();
	bool updated = false;
	// Exclusive locking is required to modify the value.
	const bool locked = compass->try_acquire_lock_exclusive(connection);
	if(locked){
		try {
			// Update the value safely.
			compass->set_value(STD_MOVE(req.value_new));

			// If shared locking semantics was requested, guarantee it.
			if(shared_lock_disposition < 0){
				
				compass->release_lock_shared(connection);
			} else if(shared_lock_disposition == 0){
				// No-op.
			} else {
				// This will not fail unless we run out of memory.
				DEBUG_THROW_ASSERT(compass->acquire_lock_shared(connection));
			}
			





		compass->set_value(STD_MOVE(req.value_new));
		//??
		if(exclusive_lock_disposition < 0){
			compass->release_lock_exclusive();
			compass->release_lock_exclusive();
		} else if(shared_lock_disposition == 0){
			compass->release_lock_exclusive();
		} else {
			// No-op
		}

		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			connection->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
			throw;
		}
	}
	compass->update_last_access_time();

	Protocol::Pilot::ExchangeResponse resp;
	resp.value_old = STD_MOVE(value_old);
	resp.updated   = updated;
	return resp;
}

}
}
