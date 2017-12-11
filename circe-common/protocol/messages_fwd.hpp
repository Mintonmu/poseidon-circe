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
