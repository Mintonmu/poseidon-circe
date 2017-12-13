// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_CBPP_RESPONSE_HPP_
#define CIRCE_COMMON_CBPP_RESPONSE_HPP_

#include <string>

namespace Circe {
namespace Common {

class CbppResponse {
private:
	long m_err_code;
	std::string m_err_msg;

public:
	CbppResponse(long err_code = 0, std::string err_msg = std::string());
	~CbppResponse();

public:
	long get_err_code() const NOEXCEPT {
		return m_err_code;
	}
	const std::string &get_err_msg() const NOEXCEPT {
		return m_err_msg;
	}
};

}
}

#endif
