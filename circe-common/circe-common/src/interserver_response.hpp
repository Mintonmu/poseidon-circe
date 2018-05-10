// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_RESPONSE_HPP_
#define CIRCE_COMMON_INTERSERVER_RESPONSE_HPP_

#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/stream_buffer.hpp>
#include <boost/cstdint.hpp>
#include <iosfwd>

namespace Circe {
namespace Common {

class Interserver_connection;

class Interserver_response {
	friend Interserver_connection;

private:
	long m_err_code;
	std::string m_err_msg;

	boost::uint64_t m_message_id;
	Poseidon::Stream_buffer m_payload;

public:
	Interserver_response();
	Interserver_response(long err_code, std::string err_msg = std::string());
	Interserver_response(const Poseidon::Cbpp::Message_base &msg);
	~Interserver_response();

public:
	long get_err_code() const NOEXCEPT {
		return m_err_code;
	}
	void set_err_code(long err_code){
		m_err_code = err_code;
	}
	const std::string &get_err_msg() const NOEXCEPT {
		return m_err_msg;
	}
	std::string &get_err_msg() NOEXCEPT {
		return m_err_msg;
	}
	void set_err_msg(std::string err_msg){
		m_err_msg = STD_MOVE(err_msg);
	}

	boost::uint64_t get_message_id() const NOEXCEPT {
		return m_message_id;
	}
	void set_message_id(boost::uint64_t message_id){
		m_message_id = message_id;
	}
	const Poseidon::Stream_buffer &get_payload() const NOEXCEPT {
		return m_payload;
	}
	Poseidon::Stream_buffer &get_payload() NOEXCEPT {
		return m_payload;
	}
	void set_payload(Poseidon::Stream_buffer payload){
		m_payload = STD_MOVE(payload);
	}

	void swap(Interserver_response &rhs) NOEXCEPT {
		using std::swap;
		swap(m_err_code,   rhs.m_err_code);
		swap(m_err_msg,    rhs.m_err_msg);
		swap(m_message_id, rhs.m_message_id);
		swap(m_payload,    rhs.m_payload);
	}
};

inline void swap(Interserver_response &lhs, Interserver_response &rhs) NOEXCEPT {
	lhs.swap(rhs);
}

extern std::ostream &operator<<(std::ostream &os, const Interserver_response &rhs);

}
}

#endif
