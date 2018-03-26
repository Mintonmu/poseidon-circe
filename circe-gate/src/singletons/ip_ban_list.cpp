// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "ip_ban_list.hpp"
#include "../mmain.hpp"
#include <poseidon/ip_port.hpp>
#include <poseidon/multi_index_map.hpp>

namespace Circe {
namespace Gate {

namespace {
	enum {
		counter_http_request,
		counter_websocket_request,
		counter_auth_failure,
		counter_end
	};

	struct Ip_element {
		// Indices.
		Poseidon::Ip_port ip_port;
		boost::uint64_t expiry_time;
		// Variables.
		mutable boost::array<boost::uint64_t, counter_end> counters;
		mutable boost::uint64_t ban_expiry_time;
	};
	MULTI_INDEX_MAP(Ip_container, Ip_element,
		UNIQUE_MEMBER_INDEX(ip_port)
		MULTI_MEMBER_INDEX(expiry_time)
	);

	Poseidon::Mutex g_mutex;
	boost::weak_ptr<Ip_container> g_weak_ip_container;

	void accumulate_and_check_ban(const char *ip, unsigned counter_index, boost::uint64_t counter_value_max){
		PROFILE_ME;

		const Poseidon::Mutex::Unique_lock lock(g_mutex);
		const AUTO(ip_container, g_weak_ip_container.lock());
		if(!ip_container){
			LOG_CIRCE_WARNING("Ip_ban_list has not been initialized.");
			DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Ip_ban_list has not been initialized"));
		}
		// Erase expired elements.
		const AUTO(now, Poseidon::get_fast_mono_clock());
		ip_container->erase<1>(ip_container->begin<1>(), ip_container->upper_bound<1>(now));
		// Find the element for this ip.
		const Poseidon::Ip_port key(ip, 0);
		AUTO(it, ip_container->find<0>(key));
		if(it == ip_container->end<0>()){
			// Create one if it doesn't exist.
			const AUTO(expiry_time, Poseidon::saturated_add<boost::uint64_t>(now, 60000));
			Ip_element elem = { key, expiry_time };
			it = ip_container->insert<0>(STD_MOVE(elem)).first;
		}
		// Increment the counter and check whether it exceeds the maximum value.
		const AUTO(counter_value_new, Poseidon::checked_add<boost::uint64_t>(it->counters.at(counter_index), 1));
		it->counters.at(counter_index) = counter_value_new;
		if(counter_value_new >= counter_value_max){
			const AUTO(ban_expiry_duration, get_config<boost::uint64_t>("client_generic_auto_ip_ban_expiry_duration", 60000));
			LOG_CIRCE_WARNING("Banning IP automatically: ip = ", it->ip_port.ip(), ", ban_expiry_duration = ", ban_expiry_duration);
			const AUTO(expiry_time, Poseidon::saturated_add(now, ban_expiry_duration));
			ip_container->set_key<0, 1>(it, expiry_time);
			it->ban_expiry_time = expiry_time;
			DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Excess flood"));
		}
	}
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(ip_container, boost::make_shared<Ip_container>());
	handles.push(ip_container);
	g_weak_ip_container = ip_container;
}

boost::uint64_t Ip_ban_list::get_ban_time_remaining(const char *ip){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("Ip_ban_list has not been initialized.");
		return 1000; // Do not allow any client to login in this case.
	}
	const Poseidon::Ip_port key(ip, 0);
	const AUTO(it, ip_container->find<0>(key));
	if(it == ip_container->end<0>()){
		return 0;
	}
	const AUTO(now, Poseidon::get_fast_mono_clock());
	return Poseidon::saturated_sub(it->ban_expiry_time, now);
}
void Ip_ban_list::set_ban_time_remaining(const char *ip, boost::uint64_t time_remaining){
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("Ip_ban_list has not been initialized.");
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("Ip_ban_list has not been initialized"));
	}
	// Find the element for this ip.
	const Poseidon::Ip_port key(ip, 0);
	AUTO(it, ip_container->find<0>(key));
	if(it == ip_container->end<0>()){
		// Create one if it doesn't exist.
		Ip_element elem = { key, 0 };
		it = ip_container->insert<0>(STD_MOVE(elem)).first;
	}
	const AUTO(now, Poseidon::get_fast_mono_clock());
	const AUTO(expiry_time, Poseidon::saturated_add(now, time_remaining));
	ip_container->set_key<0, 1>(it, expiry_time);
	it->ban_expiry_time = expiry_time;
}
bool Ip_ban_list::remove_ban(const char *ip) NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::Unique_lock lock(g_mutex);
	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("Ip_ban_list has not been initialized.");
		return false;
	}
	// XXX: Can it be made look better?
	Poseidon::Ip_port key;
	try {
		key = Poseidon::Ip_port(ip, 0);
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("Invalid IP address: what = ", e.what());
		return false;
	}
	const AUTO(it, ip_container->find<0>(key));
	if(it == ip_container->end<0>()){
		return false;
	}
	ip_container->erase<0>(it);
	return true;
}

void Ip_ban_list::accumulate_http_request(const char *ip){
	PROFILE_ME;

	const AUTO(counter_value_max, get_config<boost::uint64_t>("client_http_max_requests_per_minute_by_ip", 300));
	accumulate_and_check_ban(ip, counter_http_request, counter_value_max);
}
void Ip_ban_list::accumulate_websocket_request(const char *ip){
	PROFILE_ME;

	const AUTO(counter_value_max, get_config<boost::uint64_t>("client_websocket_max_requests_per_minute_by_ip", 120));
	accumulate_and_check_ban(ip, counter_websocket_request, counter_value_max);
}
void Ip_ban_list::accumulate_auth_failure(const char *ip){
	PROFILE_ME;

	const AUTO(counter_value_max, get_config<boost::uint64_t>("client_generic_max_auth_failure_count_per_minute_by_ip", 5));
	accumulate_and_check_ban(ip, counter_auth_failure, counter_value_max);
}

}
}
