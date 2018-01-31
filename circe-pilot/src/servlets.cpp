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
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_string(req.key)));
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
	DEBUG_THROW_ASSERT(!(shared_lock_disposition < 0) || compass->is_locked_shared_by(connection->get_connection_uuid()));
	DEBUG_THROW_ASSERT(!(exclusive_lock_disposition < 0) || compass->is_locked_exclusive_by(connection->get_connection_uuid()));

	std::string value_old = compass->get_value();
	unsigned version_old = compass->get_version();
	bool updated = false;
	std::size_t criterion_index = 0;

	// Search for the first match.
	while((criterion_index < req.criteria.size()) && (req.criteria.at(criterion_index).value_cmp != value_old)){
		++criterion_index;
	}
	LOG_CIRCE_DEBUG("Compass comparison result: req = ", req, ", criterion_index = ", criterion_index);

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
		try {
			// Update the value safely.
			compass->set_value(STD_MOVE(req.criteria.at(criterion_index).value_new));
			updated = true;

			// If shared locking has been requested, guarantee it.
			while(shared_lock_disposition < 0){
				compass->release_lock_shared(connection);
				++shared_lock_disposition;
			}
			while(shared_lock_disposition > 0){
				DEBUG_THROW_ASSERT(compass->try_lock_shared(connection));
				--shared_lock_disposition;
			}

			// If exclusive locking has been requested, guarantee it.
			while(exclusive_lock_disposition < 0){
				compass->release_lock_exclusive(connection);
				++exclusive_lock_disposition;
			}
			while(exclusive_lock_disposition > 0){
				DEBUG_THROW_ASSERT(compass->try_lock_exclusive(connection));
				--exclusive_lock_disposition;
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			connection->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
			throw;
		}
	}
	compass->update_last_access_time();

	Protocol::Pilot::CompareExchangeResponse resp;
	resp.value_old       = STD_MOVE(value_old);
	resp.version_old     = version_old;
	resp.updated         = updated;
	resp.criterion_index = criterion_index;
	return resp;
}

DEFINE_SERVLET_FOR(Protocol::Pilot::ExchangeRequest, connection, req){
	const AUTO(compass, CompassRepository::open_compass(CompassKey::from_string(req.key)));
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
	DEBUG_THROW_ASSERT(!(shared_lock_disposition < 0) || compass->is_locked_shared_by(connection->get_connection_uuid()));
	DEBUG_THROW_ASSERT(!(exclusive_lock_disposition < 0) || compass->is_locked_exclusive_by(connection->get_connection_uuid()));

	std::string value_old = compass->get_value();
	unsigned version_old = compass->get_version();
	bool updated = false;

	// Ask for exclusive locking.
	bool locked = compass->try_lock_exclusive(connection);
	exclusive_lock_disposition -= locked;
	// Update the value only if the value has been locked successfully.
	if(locked){
		try {
			// Update the value safely.
			compass->set_value(STD_MOVE(req.value_new));
			updated = true;

			// If shared locking has been requested, guarantee it.
			while(shared_lock_disposition < 0){
				compass->release_lock_shared(connection);
				++shared_lock_disposition;
			}
			while(shared_lock_disposition > 0){
				DEBUG_THROW_ASSERT(compass->try_lock_shared(connection));
				--shared_lock_disposition;
			}

			// If exclusive locking has been requested, guarantee it.
			while(exclusive_lock_disposition < 0){
				compass->release_lock_exclusive(connection);
				++exclusive_lock_disposition;
			}
			while(exclusive_lock_disposition > 0){
				DEBUG_THROW_ASSERT(compass->try_lock_exclusive(connection));
				--exclusive_lock_disposition;
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			connection->shutdown(Protocol::ERR_INTERNAL_ERROR, e.what());
			throw;
		}
	}
	compass->update_last_access_time();

	Protocol::Pilot::ExchangeResponse resp;
	resp.value_old   = STD_MOVE(value_old);
	resp.version_old = version_old;
	resp.updated     = updated;
	return resp;
}

}
}
