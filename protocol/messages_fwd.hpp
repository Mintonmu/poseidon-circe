// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_MESSAGES_FWD_HPP_
#define CIRCE_PROTOCOL_MESSAGES_FWD_HPP_

namespace Circe {
namespace Protocol {

// Primary -> Secondary
class PS_Connect;
class PS_Send;
class PS_Acknowledge;
class PS_Shutdown;

// Secondary -> Primary
class SP_Opened;
class SP_Connected;
class SP_Received;
class SP_Closed;

}
}

#endif
