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
	bool updated = false;

	// Exclusive locking is required to modify the value.
	const bool locked_exclusive = compass->try_lock_exclusive(connection);
	if(locked_exclusive){
		// This exclusive lock has to be released once.
		--exclusive_lock_disposition;

		try {
			// Update the value safely.
			compass->set_value(STD_MOVE(req.value_new));

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
	resp.value_old = STD_MOVE(value_old);
	resp.updated   = updated;
	return resp;
}

}
}
