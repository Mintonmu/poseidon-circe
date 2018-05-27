// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_response.hpp"
#include "protocol/error_codes.hpp"

namespace Circe {
namespace Common {

Interserver_response::Interserver_response()
	: m_err_code(Protocol::error_reserved_response_uninitialized), m_err_msg()
	, m_message_id(0xDEADBEEF), m_payload()
{
	//
}
Interserver_response::Interserver_response(long err_code, std::string err_msg)
	: m_err_code(err_code), m_err_msg(STD_MOVE(err_msg))
	, m_message_id(), m_payload()
{
	CIRCE_LOG_TRACE("Constructing Interserver_response without a payload: err_code = ", get_err_code(), ", err_msg = ", get_err_msg());
}
Interserver_response::Interserver_response(const Poseidon::Cbpp::Message_base &msg)
	: m_err_code(Protocol::error_success), m_err_msg()
	, m_message_id(msg.get_id()), m_payload(msg)
{
	CIRCE_LOG_TRACE("Constructing Interserver_response from message: msg = ", msg);
}
Interserver_response::~Interserver_response(){
	//
}

// Non-member functions.
std::ostream &operator<<(std::ostream &os, const Interserver_response &rhs){
	os <<"Interserver_response(";
	if(rhs.get_message_id() == 0){
		os <<"err_code = " <<rhs.get_err_code() <<", err_msg = " <<rhs.get_err_msg();
	} else {
		os <<"message_id = " <<rhs.get_message_id() <<", payload_size = " <<rhs.get_payload().size();
	}
	os <<")";
	return os;
}

}
}
