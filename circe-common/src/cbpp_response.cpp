// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "cbpp_response.hpp"
#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/cbpp/status_codes.hpp>

namespace Circe {
namespace Common {

CbppResponse::CbppResponse(long err_code, std::string err_msg)
	: m_err_code(err_code), m_err_msg(STD_MOVE(err_msg))
	, m_message_id(), m_payload()
{ }
CbppResponse::CbppResponse(boost::uint64_t message_id, Poseidon::StreamBuffer payload)
	: m_err_code(Poseidon::Cbpp::ST_OK), m_err_msg()
	, m_message_id(message_id), m_payload(STD_MOVE(payload))
{ }
CbppResponse::CbppResponse(const Poseidon::Cbpp::MessageBase &msg)
	: m_err_code(Poseidon::Cbpp::ST_OK), m_err_msg()
	, m_message_id(msg.get_id()), m_payload(msg)
{ }
CbppResponse::~CbppResponse(){ }

}
}
