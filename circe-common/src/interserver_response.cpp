// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_response.hpp"
#include "protocol/error_codes.hpp"

namespace Circe {
namespace Common {

InterserverResponse::InterserverResponse()
	: m_err_code(Protocol::ERR_RESERVED_RESPONSE_UNINITIALIZED), m_err_msg()
	, m_message_id(0xDEADBEEF), m_payload()
{
	//
}
InterserverResponse::InterserverResponse(long err_code, std::string err_msg)
	: m_err_code(err_code), m_err_msg(STD_MOVE(err_msg))
	, m_message_id(), m_payload()
{
	LOG_CIRCE_TRACE("Constructing InterserverResponse without a payload: err_code = ", get_err_code(), ", err_msg = ", get_err_msg());
}
InterserverResponse::InterserverResponse(const Poseidon::Cbpp::MessageBase &msg)
	: m_err_code(Protocol::ERR_SUCCESS), m_err_msg()
	, m_message_id(msg.get_id()), m_payload(msg)
{
	LOG_CIRCE_TRACE("Constructing InterserverResponse from message: msg = ", msg);
}
InterserverResponse::~InterserverResponse(){
	//
}

// Non-member functions.
std::ostream &operator<<(std::ostream &os, const InterserverResponse &rhs){
	os <<"InterserverResponse(";
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
