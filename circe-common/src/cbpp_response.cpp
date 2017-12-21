// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "cbpp_response.hpp"

namespace Circe {
namespace Common {

CbppResponse::CbppResponse(long err_code, std::string err_msg)
	: m_err_code(err_code), m_err_msg(STD_MOVE(err_msg))
	, m_message_id(), m_payload()
{
	LOG_CIRCE_DEBUG("Constructing an empty response: err_code = ", get_err_code(), ", err_msg = ", get_err_msg());
}
CbppResponse::CbppResponse(boost::uint64_t message_id, Poseidon::StreamBuffer payload)
	: m_err_code(Protocol::ERR_SUCCESS), m_err_msg()
	, m_message_id(message_id), m_payload(STD_MOVE(payload))
{
	LOG_CIRCE_DEBUG("Constructing response from message: message_id = ", message_id, ", payload.size() = ", payload.size());
}
CbppResponse::CbppResponse(const Poseidon::Cbpp::MessageBase &msg)
	: m_err_code(Protocol::ERR_SUCCESS), m_err_msg()
	, m_message_id(msg.get_id()), m_payload(msg)
{
	LOG_CIRCE_DEBUG("Constructing response from message: msg = ", msg);
}
CbppResponse::~CbppResponse(){ }

}
}
