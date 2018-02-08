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

DEFINE_SERVLET_FOR(Protocol::Pilot::CompareExchangeRequest, connection, req){
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_hash_of(req.key.data(), req.key.size())));
	DEBUG_THROW_ASSERT(compass);
	LOG_CIRCE_DEBUG("Opened compass: ", compass->get_compass_key());

	int shared_lock_disposition;
	int exclusive_lock_disposition;
	switch(req.lock_disposition){
	case Protocol::Pilot::LOCK_LEAVE_ALONE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_SHARED:
		shared_lock_disposition = 1;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_EXCLUSIVE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = 1;
		break;
	case Protocol::Pilot::LOCK_RELEASE_SHARED:
		shared_lock_disposition = -1;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_RELEASE_EXCLUSIVE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = -1;
		break;
	default:
		LOG_CIRCE_ERROR("Unknown lock_disposition: ", req.lock_disposition);
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Unknown lock_disposition"));
	}
	if(shared_lock_disposition < 0){
		DEBUG_THROW_ASSERT(compass->is_locked_shared_by(connection->get_connection_uuid()));
	}
	if(exclusive_lock_disposition < 0){
		DEBUG_THROW_ASSERT(compass->is_locked_exclusive_by(connection->get_connection_uuid()));
	}

	std::string value_old = compass->get_value();
	unsigned version_old = compass->get_version();
	bool succeeded = false;
	std::size_t criterion_index = 0;
	unsigned lock_state = Protocol::Pilot::LOCK_FREE_FOR_ACQUISITION;

	// Search for the first match.
	while((criterion_index < req.criteria.size()) && (req.criteria.at(criterion_index).value_cmp != value_old)){
		++criterion_index;
	}
	LOG_CIRCE_DEBUG("Compass comparison result: criterion_index = ", criterion_index);

	bool locked;
	if(!req.criteria.empty() && (criterion_index >= req.criteria.size())){
		// Don't lock the value if the comparison fails.
		locked = false;
	} else if(req.criteria.empty() && (exclusive_lock_disposition <= 0)){
		// Ask for shared locking if the value isn't to be modified.
		locked = compass->try_lock_shared(connection);
		shared_lock_disposition -= locked;
	} else {
		// Otherwise, ask for exclusive locking.
		locked = compass->try_lock_exclusive(connection);
		exclusive_lock_disposition -= locked;
	}
	// Update the value only if the value has been locked successfully.
	if(locked){
		if(criterion_index < req.criteria.size()){
			// Update the value safely.
			compass->set_value(STD_MOVE(req.criteria.at(criterion_index).value_new));
		}
		succeeded = true;
		// If shared locking has been requested, guarantee it.
		while(shared_lock_disposition < 0){
			DEBUG_THROW_ASSERT(compass->release_lock_shared(connection));
			++shared_lock_disposition;
		}
		while(shared_lock_disposition > 0){
			DEBUG_THROW_ASSERT(compass->try_lock_shared(connection));
			--shared_lock_disposition;
		}
		// If exclusive locking has been requested, guarantee it.
		while(exclusive_lock_disposition < 0){
			DEBUG_THROW_ASSERT(compass->release_lock_exclusive(connection));
			++exclusive_lock_disposition;
		}
		while(exclusive_lock_disposition > 0){
			DEBUG_THROW_ASSERT(compass->try_lock_exclusive(connection));
			--exclusive_lock_disposition;
		}
	}
	if(compass->is_locked_exclusive_by(connection->get_connection_uuid())){
		lock_state = Protocol::Pilot::LOCK_EXCLUSIVE_BY_ME;
	} else if(compass->is_locked_exclusive()){
		lock_state = Protocol::Pilot::LOCK_EXCLUSIVE_BY_OTHERS;
	} else if(compass->is_locked_shared_by(connection->get_connection_uuid())){
		lock_state = Protocol::Pilot::LOCK_SHARED_BY_ME;
	} else if(compass->is_locked_shared()){
		lock_state = Protocol::Pilot::LOCK_SHARED_BY_OTHERS;
	}
	compass->update_last_access_time();

	Protocol::Pilot::CompareExchangeResponse resp;
	resp.value_old       = STD_MOVE(value_old);
	resp.version_old     = version_old;
	resp.succeeded       = succeeded;
	resp.criterion_index = criterion_index;
	resp.lock_state      = lock_state;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Pilot::ExchangeRequest, connection, req){
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_hash_of(req.key.data(), req.key.size())));
	DEBUG_THROW_ASSERT(compass);
	LOG_CIRCE_DEBUG("Opened compass: ", compass->get_compass_key());

	int shared_lock_disposition;
	int exclusive_lock_disposition;
	switch(req.lock_disposition){
	case Protocol::Pilot::LOCK_LEAVE_ALONE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_SHARED:
		shared_lock_disposition = 1;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_TRY_ACQUIRE_EXCLUSIVE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = 1;
		break;
	case Protocol::Pilot::LOCK_RELEASE_SHARED:
		shared_lock_disposition = -1;
		exclusive_lock_disposition = 0;
		break;
	case Protocol::Pilot::LOCK_RELEASE_EXCLUSIVE:
		shared_lock_disposition = 0;
		exclusive_lock_disposition = -1;
		break;
	default:
		LOG_CIRCE_ERROR("Unknown lock_disposition: ", req.lock_disposition);
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Unknown lock_disposition"));
	}
	if(shared_lock_disposition < 0){
		DEBUG_THROW_ASSERT(compass->is_locked_shared_by(connection->get_connection_uuid()));
	}
	if(exclusive_lock_disposition < 0){
		DEBUG_THROW_ASSERT(compass->is_locked_exclusive_by(connection->get_connection_uuid()));
	}

	std::string value_old = compass->get_value();
	unsigned version_old = compass->get_version();
	bool succeeded = false;
	unsigned lock_state = Protocol::Pilot::LOCK_FREE_FOR_ACQUISITION;

	// Ask for exclusive locking.
	bool locked = compass->try_lock_exclusive(connection);
	exclusive_lock_disposition -= locked;
	// Update the value only if the value has been locked successfully.
	if(locked){
		// Update the value safely.
		compass->set_value(STD_MOVE(req.value_new));
		succeeded = true;
		// If shared locking has been requested, guarantee it.
		while(shared_lock_disposition < 0){
			DEBUG_THROW_ASSERT(compass->release_lock_shared(connection));
			++shared_lock_disposition;
		}
		while(shared_lock_disposition > 0){
			DEBUG_THROW_ASSERT(compass->try_lock_shared(connection));
			--shared_lock_disposition;
		}
		// If exclusive locking has been requested, guarantee it.
		while(exclusive_lock_disposition < 0){
			DEBUG_THROW_ASSERT(compass->release_lock_exclusive(connection));
			++exclusive_lock_disposition;
		}
		while(exclusive_lock_disposition > 0){
			DEBUG_THROW_ASSERT(compass->try_lock_exclusive(connection));
			--exclusive_lock_disposition;
		}
	}
	if(compass->is_locked_exclusive_by(connection->get_connection_uuid())){
		lock_state = Protocol::Pilot::LOCK_EXCLUSIVE_BY_ME;
	} else if(compass->is_locked_exclusive()){
		lock_state = Protocol::Pilot::LOCK_EXCLUSIVE_BY_OTHERS;
	} else if(compass->is_locked_shared_by(connection->get_connection_uuid())){
		lock_state = Protocol::Pilot::LOCK_SHARED_BY_ME;
	} else if(compass->is_locked_shared()){
		lock_state = Protocol::Pilot::LOCK_SHARED_BY_OTHERS;
	}
	compass->update_last_access_time();

	Protocol::Pilot::ExchangeResponse resp;
	resp.value_old   = STD_MOVE(value_old);
	resp.version_old = version_old;
	resp.succeeded   = succeeded;
	resp.lock_state  = lock_state;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Pilot::AddWatchRequest, connection, req){
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_hash_of(req.key.data(), req.key.size())));
	DEBUG_THROW_ASSERT(compass);
	LOG_CIRCE_DEBUG("Opened compass: ", compass->get_compass_key());

	const AUTO(watcher_uuid, compass->add_watcher(connection));

	Protocol::Pilot::AddWatchResponse resp;
	resp.watcher_uuid = watcher_uuid;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Pilot::RemoveWatchNotification, /*connection*/, ntfy){
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_hash_of(ntfy.key.data(), ntfy.key.size())));
	DEBUG_THROW_ASSERT(compass);
	LOG_CIRCE_DEBUG("Opened compass: ", compass->get_compass_key());

	const AUTO(watcher_uuid, Poseidon::Uuid(ntfy.watcher_uuid));
	compass->remove_watcher(watcher_uuid);

	return Protocol::ERR_SUCCESS;
}

}
}
