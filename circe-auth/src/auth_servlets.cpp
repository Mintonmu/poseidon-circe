// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "singletons/auth_acceptor.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"

#define DEFINE_SERVLET(Msg_, conn_param_, msg_param_)	\
	namespace {	\
		namespace Servlets_ {	\
			::Circe::Common::CbppResponse TOKEN_CAT2(servlet_callback_, __LINE__)(const ::boost::shared_ptr< ::Circe::Common::InterserverConnection> & conn_, Msg_ msg_);	\
			::Circe::Common::CbppResponse TOKEN_CAT2(servlet_wrapper_, __LINE__)(const ::boost::shared_ptr< ::Circe::Common::InterserverConnection> & conn_, ::boost::uint16_t msgid_, ::Poseidon::StreamBuffer payload_){	\
				PROFILE_ME;	\
				DEBUG_THROW_ASSERT(msgid_ == Msg_::ID);	\
				Msg_ msg_(payload_);	\
				LOG_CIRCE_TRACE("Processing: ", msg_);	\
				return TOKEN_CAT2(servlet_callback_, __LINE__)(conn_, STD_MOVE_IDN(msg_));	\
			}	\
		}	\
		MODULE_RAII_PRIORITY(handles_, INIT_PRIORITY_LOW){	\
			const AUTO(servlet_, ::boost::make_shared< ::Circe::Common::InterserverServletCallback>(&Servlets_::TOKEN_CAT2(servlet_wrapper_, __LINE__)));	\
			LOG_CIRCE_DEBUG("Registering servlet: ", #Msg_);	\
			::Circe::Auth::AuthAcceptor::insert_servlet(Msg_::ID, servlet_);	\
			handles_.push(servlet_);	\
		}	\
	}	\
	::Circe::Common::CbppResponse Servlets_::TOKEN_CAT2(servlet_callback_, __LINE__)(const ::boost::shared_ptr< ::Circe::Common::InterserverConnection> & conn_param_, Msg_ msg_param_)

namespace Circe {
namespace Auth {

DEFINE_SERVLET(Protocol::GA_ClientHttpAuthenticationRequest, connection, req){
	LOG_POSEIDON_FATAL("TODO: ", req);

	Protocol::AG_ClientHttpAuthenticationResponse resp;
	return resp;
}

DEFINE_SERVLET(Protocol::GA_ClientWebSocketAuthenticationRequest, connection, req){
	LOG_POSEIDON_FATAL("TODO: ", req);

	Protocol::AG_ClientWebSocketAuthenticationResponse resp;
	return resp;
}

}
}
