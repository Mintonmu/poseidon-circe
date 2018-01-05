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
	class IpContainer {
	public:
		enum {
			COUNTER_HTTP_REQUEST,
			COUNTER_WEBSOCKET_REQUEST,
			COUNTER_AUTH_FAILURE,
			COUNTER_END
		};

	private:
		struct Element {
			// Indices.
			Poseidon::IpPort ip_port;
			boost::uint64_t expiry_time;
			// Variants.
			mutable boost::array<boost::uint64_t, COUNTER_END> counters;
			mutable boost::uint64_t ban_expiry_time;
		};
		MULTI_INDEX_MAP(Map, Element,
			UNIQUE_MEMBER_INDEX(ip_port)
			MULTI_MEMBER_INDEX(expiry_time)
		);

	private:
		mutable Poseidon::Mutex m_mutex;
		Map m_map;

	public:
		IpContainer()
			: m_map()
		{ }

	public:
		boost::uint64_t get_ban_time_remaining(const char *ip) const {
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			const AUTO(now, Poseidon::get_fast_mono_clock());
			const Poseidon::IpPort key(ip, 0);
			const AUTO(it, m_map.find<0>(key));
			if(it == m_map.end<0>()){
				return 0;
			}
			return Poseidon::saturated_sub(it->ban_expiry_time, now);
		}
		void increment_counter(const char *ip, unsigned counter_index, boost::uint64_t counter_max_value){
			PROFILE_ME;

			const Poseidon::Mutex::UniqueLock lock(m_mutex);
			const AUTO(now, Poseidon::get_fast_mono_clock());
			// Erase expired elements.
			m_map.erase<1>(m_map.begin<1>(), m_map.upper_bound<1>(now));
			// Find the element for the current ip. Create one if no one exists.
			const Poseidon::IpPort key(ip, 0);
			AUTO(it, m_map.find<0>(key));
			if(it == m_map.end<0>()){
				Element elem = { key, Poseidon::saturated_add<boost::uint64_t>(now, 60000) };
				it = m_map.insert<0>(STD_MOVE(elem)).first;
			}
			if(it->counters.at(counter_index)++ < counter_max_value){
				return;
			}
			const AUTO(expiry_duration, get_config<boost::uint64_t>("client_generic_auto_ip_ban_expiry_duration", 60000));
			LOG_CIRCE_WARNING("Banning IP automatically: ip = ", it->ip_port.ip());
			m_map.set_key<0, 1>(it, Poseidon::saturated_add(now, expiry_duration));
			it->ban_expiry_time = it->expiry_time;
		}
	};

	boost::weak_ptr<IpContainer> g_weak_ip_container;
}

MODULE_RAII_PRIORITY(handles, INIT_PRIORITY_ESSENTIAL){
	const AUTO(ip_container, boost::make_shared<IpContainer>());
	handles.push(ip_container);
	g_weak_ip_container = ip_container;
}

boost::uint64_t IpBanList::get_ban_time_remaining(const char *ip){
	PROFILE_ME;

	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("IpBanList has not been initialized.");
		return 1000; // Do not allow any client to login in this case.
	}
	return ip_container->get_ban_time_remaining(ip);
}

void IpBanList::accumulate_http_request(const char *ip){
	PROFILE_ME;

	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("IpBanList has not been initialized.");
		return;
	}
	const AUTO(counter_max_value, get_config<boost::uint64_t>("client_http_max_requests_per_minute_by_ip", 300));
	return ip_container->increment_counter(ip, IpContainer::COUNTER_HTTP_REQUEST, counter_max_value);
}
void IpBanList::accumulate_websocket_request(const char *ip){
	PROFILE_ME;

	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("IpBanList has not been initialized.");
		return;
	}
	const AUTO(counter_max_value, get_config<boost::uint64_t>("client_websocket_max_requests_per_minute_by_ip", 120));
	return ip_container->increment_counter(ip, IpContainer::COUNTER_WEBSOCKET_REQUEST, counter_max_value);
}
void IpBanList::accumulate_auth_failure(const char *ip){
	PROFILE_ME;

	const AUTO(ip_container, g_weak_ip_container.lock());
	if(!ip_container){
		LOG_CIRCE_WARNING("IpBanList has not been initialized.");
		return;
	}
	const AUTO(counter_max_value, get_config<boost::uint64_t>("client_generic_max_auth_failure_count_per_minute_by_ip", 5));
	return ip_container->increment_counter(ip, IpContainer::COUNTER_AUTH_FAILURE, counter_max_value);
}

}
}
