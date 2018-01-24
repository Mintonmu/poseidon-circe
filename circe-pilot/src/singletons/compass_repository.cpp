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
	struct CompassElement {
		// Invariants.
		boost::shared_ptr<Compass> compass;
		// Indices.
		const volatile Compass *ptr;
		CompassKey compass_key;
		boost::uint64_t last_access_time;
		// Variables.
	};
	MULTI_INDEX_MAP(CompassContainer, CompassElement,
		UNIQUE_MEMBER_INDEX(ptr)
		UNIQUE_MEMBER_INDEX(compass_key)
		MULTI_MEMBER_INDEX(last_access_time)
	);

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<CompassContainer> g_weak_compass_container;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	const AUTO(compass_container, boost::make_shared<CompassContainer>());
	handles.push(compass_container);
	g_weak_compass_container = compass_container;
}

namespace {
	void gc_timer_proc(){
		PROFILE_ME;

		const AUTO(utc_now, Poseidon::get_utc_time());
		const AUTO(expiry_duration, get_config<boost::uint64_t>("compass_expiry_duration", 86400000));
		const AUTO(expiry_time, Poseidon::saturated_add(utc_now, expiry_duration));

		// Delete expired compasses from the database...
		Poseidon::Buffer_ostream sql_os;
		sql_os <<"DELETE FROM `Pilot::Compass` WHERE `last_access_time` < " <<Poseidon::MySql::DateTimeFormatter(expiry_time);
		Poseidon::MySqlDaemon::enqueue_for_deleting("Pilot::Compass", sql_os.get_buffer().dump_string());

		// ... as well as those in memory.
//		const AUTO(compass_container, g_weak_compass_container.lock());
//		if(compass_container){
//			return;
//		}

//		const Poseidon::Mutex::UniqueLock lock(g_mutex);
//		compass_container->
	}
}
MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_LOW){
	const AUTO(gc_timer_interval, get_config<boost::uint64_t>("compass_gc_timer_interval", 60000));
	const AUTO(timer, Poseidon::TimerDaemon::register_timer(0, gc_timer_interval, std::bind(&gc_timer_proc)));
	handles.push(timer);
}

bool CompassRepository::update_compass(const volatile Compass *compass) NOEXCEPT {
	return { };
}

boost::shared_ptr<Compass> CompassRepository::get_compass(const CompassKey &compass_key){
	return { };
}
boost::shared_ptr<Compass> CompassRepository::open_compass(const CompassKey &compass_key){
	return { };
}
void CompassRepository::insert_compass(const boost::shared_ptr<Compass> &compass){
	return ;
}
bool CompassRepository::remove_compass(const volatile Compass *compass) NOEXCEPT {
	return { };
}

}
}
