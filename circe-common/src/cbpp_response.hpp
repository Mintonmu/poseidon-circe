// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_CBPP_RESPONSE_HPP_
#define CIRCE_COMMON_CBPP_RESPONSE_HPP_

#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/stream_buffer.hpp>
#include <boost/cstdint.hpp>

namespace Circe {
namespace Common {

class CbppResponse {
private:
	long m_status_code;
	boost::uint64_t m_message_id;
	Poseidon::StreamBuffer m_payload;

public:
	CbppResponse(long status_code = 0);
	CbppResponse(boost::uint64_t message_id, Poseidon::StreamBuffer payload);
	CbppResponse(const Poseidon::Cbpp::MessageBase &msg);
	~CbppResponse();

public:
	long get_status_code() const NOEXCEPT {
		return m_status_code;
	}
	const Poseidon::StreamBuffer &get_payload() const NOEXCEPT {
		return m_payload;
	}
	Poseidon::StreamBuffer &get_payload() NOEXCEPT {
		return m_payload;
	}
};

}
}

#endif
