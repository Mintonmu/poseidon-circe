// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_CBPP_RESPONSE_HPP_
#define CIRCE_COMMON_CBPP_RESPONSE_HPP_

#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/stream_buffer.hpp>
#include <boost/cstdint.hpp>
#include <iosfwd>

namespace Circe {
namespace Common {

class InterserverConnection;

class CbppResponse {
	friend InterserverConnection;

private:
	long m_err_code;
	std::string m_err_msg;

	boost::uint64_t m_message_id;
	Poseidon::StreamBuffer m_payload;

private:
	CbppResponse(long err_code, std::string err_msg, boost::uint64_t message_id, Poseidon::StreamBuffer payload)
		: m_err_code(err_code), m_err_msg(STD_MOVE(err_msg)), m_message_id(message_id), m_payload(STD_MOVE(payload))
	{ }

public:
	CbppResponse(long err_code, std::string err_msg = std::string());
	CbppResponse(const Poseidon::Cbpp::MessageBase &msg);
	~CbppResponse();

public:
	long get_err_code() const NOEXCEPT {
		return m_err_code;
	}
	const std::string &get_err_msg() const NOEXCEPT {
		return m_err_msg;
	}
	std::string &get_err_msg() NOEXCEPT {
		return m_err_msg;
	}

	boost::uint64_t get_message_id() const NOEXCEPT {
		return m_message_id;
	}
	const Poseidon::StreamBuffer &get_payload() const NOEXCEPT {
		return m_payload;
	}
	Poseidon::StreamBuffer &get_payload() NOEXCEPT {
		return m_payload;
	}
};

extern std::ostream &operator<<(std::ostream &os, const CbppResponse &rhs);

}
}

#endif
