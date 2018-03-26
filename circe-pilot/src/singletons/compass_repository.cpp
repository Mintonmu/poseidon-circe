// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_repository.hpp"
#include "../mmain.hpp"
#include "../orm.hpp"
#include <poseidon/multi_index_map.hpp>
#include <poseidon/singletons/mysql_daemon.hpp>
#include <poseidon/singletons/timer_daemon.hpp>

namespace Circe {
namespace Pilot {

namespace {
	struct Compass_element {
		// Invariants.
		boost::shared_ptr<Compass> compass;
		// Indices.
		const volatile Compass *ptr;
		Compass_key compass_key;
		boost::uint64_t last_access_time;
		// Variables.
	};
	Compass_element create_compass_element(const boost::shared_ptr<Compass> &compass){
		Compass_element elem = { compass, compass.get(), compass->get_compass_key(), compass->get_last_access_time() };
		return elem;
	}
	MULTI_INDEX_MAP(Compass_container, Compass_element,
		UNIQUE_MEMBER_INDEX(ptr)
		UNIQUE_MEMBER_INDEX(compass_key)
		MULTI_MEMBER_INDEX(last_access_time)
	);

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Compass_container> g_weak_compass_container;

	void gc_timer_proc(){
		PROFILE_ME;

		const AUTO(utc_now, Poseidon::get_utc_time());
		const AUTO(expiry_duration, get_config<boost::uint64_t>("compass_expiry_duration", 86400000));
		const AUTO(expiry_time, Poseidon::saturated_sub(utc_now, expiry_duration));
		// Delete expired compasses from memory...
		try {
			const Poseidon::Mutex::Unique_lock lock(g_mutex);
			const AUTO(compass_container, g_weak_compass_container.lock());
			if(compass_container){
				compass_container->erase<2>(compass_container->begin<2>(), compass_container->upper_bound<2>(expiry_time));
			}
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		}
		// ... and from database.
		try {
			Poseidon::Buffer_ostream sql_os;
			sql_os <<"DELETE FROM `Pilot::Compass` WHERE `last_access_time` <= " <<Poseidon::My_sql::Date_time_formatter(expiry_time);
			Poseidon::My_sql_daemon::enqueue_for_deleting("Pilot::Compass", sql_os.get_buffer().dump_string());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
		}
	}
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(mysql_conn, Poseidon::My_sql_daemon::create_connection());
	const AUTO(compass_container, boost::make_shared<Compass_container>());
	LOG_CIRCE_INFO("Loading compasses from MySQL master...");
	mysql_conn->execute_sql("SELECT * FROM `Pilot::Compass`");
	while(mysql_conn->fetch_row()){
		const AUTO(dao, boost::make_shared<ORM_Compass>());
		dao->fetch(mysql_conn);
		DEBUG_THROW_ASSERT(compass_container->insert(create_compass_element(boost::make_shared<Compass>(dao))).second);
	}
	LOG_CIRCE_INFO("Finished loading compasses from MySQL master.");
	handles.push(compass_container);
	g_weak_compass_container = compass_container;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const AUTO(gc_timer_interval, get_config<boost::uint64_t>("compass_gc_timer_interval", 60000));
	const AUTO(timer, Poseidon::Timer_daemon::register_timer(0, gc_timer_interval, std::bind(&gc_timer_proc)));
	handles.push(timer);
}

bool Compass_repository::update_compass_indices(const volatile Compass *ptr) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(compass_container, g_weak_compass_container.lock());
	if(!compass_container){
		LOG_CIRCE_WARNING("Compass_repository has not been initialized.");
		return false;
	}
	const AUTO(it, compass_container->find<0>(ptr));
	if(it == compass_container->end<0>()){
		LOG_CIRCE_DEBUG("Compass not found: ptr = ", (void *)ptr);
		return false;
	}
	compass_container->replace<0>(it, create_compass_element(it->compass));
	return true;
}

boost::shared_ptr<Compass> Compass_repository::get_compass(const Compass_key &compass_key){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(compass_container, g_weak_compass_container.lock());
	if(!compass_container){
		LOG_CIRCE_WARNING("Compass_repository has not been initialized.");
		return VAL_INIT;
	}
	const AUTO(it, compass_container->find<1>(compass_key));
	if(it == compass_container->end<1>()){
		LOG_CIRCE_DEBUG("Compass not found: compass_key = ", compass_key);
		return VAL_INIT;
	}
	return it->compass;
}
boost::shared_ptr<Compass> Compass_repository::open_compass(const Compass_key &compass_key){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(compass_container, g_weak_compass_container.lock());
	if(!compass_container){
		LOG_CIRCE_WARNING("Compass_repository has not been initialized.");
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Compass_repository has not been initialized"));
	}
	AUTO(it, compass_container->find<1>(compass_key));
	if(it == compass_container->end<1>()){
		LOG_CIRCE_DEBUG("Creating new compass: compass_key = ", compass_key);
		it = compass_container->insert<1>(create_compass_element(boost::make_shared<Compass>(compass_key))).first;
	}
	return it->compass;
}
bool Compass_repository::remove_compass(const volatile Compass *ptr) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(compass_container, g_weak_compass_container.lock());
	if(!compass_container){
		LOG_CIRCE_WARNING("Compass_repository has not been initialized.");
		return false;
	}
	const AUTO(it, compass_container->find<0>(ptr));
	if(it == compass_container->end<0>()){
		LOG_CIRCE_DEBUG("Compass not found: ptr = ", (void *)ptr);
		return false;
	}
	compass_container->erase<0>(it);
	return true;
}

}
}
