// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_SINGLETONS_IP_BAN_LIST_HPP_
#define CIRCE_GATE_SINGLETONS_IP_BAN_LIST_HPP_

#include <boost/cstdint.hpp>

namespace Circe {
namespace Gate {

class IpBanList {
private:
	IpBanList();

public:
	static boost::uint64_t get_ban_time_remaining(const char *ip);

	static void accumulate_http_request(const char *ip);
	static void accumulate_websocket_request(const char *ip);
	static void accumulate_auth_failure(const char *ip);
};

}
}

#endif
