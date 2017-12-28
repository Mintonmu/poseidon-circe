// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_DEFINE_INTERSERVER_SERVLET_HPP_
#define CIRCE_COMMON_DEFINE_INTERSERVER_SERVLET_HPP_

#include "interserver_servlet_callback.hpp"
#include "log.hpp"
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/cxx_util.hpp>
#include <poseidon/profiler.hpp>
#include <poseidon/module_raii.hpp>

namespace Circe {
namespace Common {

template<typename R, typename C, typename M> M guess_message_type(R (*)(const C &, M));

}
}

#define CIRCE_DEFINE_INTERSERVER_SERVLET(insert_servlet_, ...)	\
	namespace {	\
		/* Define the servlet wrapper. We need some local definitions inside it. */	\
		struct TOKEN_CAT2(InterserverServletWrapper_, __LINE__) {	\
			/* Declare the user-defined callback. */	\
			static ::Circe::Common::CbppResponse unwrapped_callback_(__VA_ARGS__);	\
			/* Guess the message type. */	\
			typedef VALUE_TYPE(::Circe::Common::guess_message_type(&unwrapped_callback_)) Message_;	\
			/* This is the callback that matches `InterserverServletCallback`. */	\
			static ::Circe::Common::CbppResponse callback_(const ::boost::shared_ptr< ::Circe::Common::InterserverConnection> &connection_, ::boost::uint16_t message_id_, ::Poseidon::StreamBuffer payload_){	\
				PROFILE_ME;	\
				DEBUG_THROW_ASSERT(message_id_ == Message_::ID);	\
				Message_ req_(STD_MOVE(payload_));	\
				LOG_CIRCE_DEBUG("Received request from ", connection_->get_remote_info(), ": ", req_);	\
				return unwrapped_callback_(connection_, STD_MOVE(req_));	\
			}	\
		};	\
		/* Register the wrapped callback upon module load. */	\
		MODULE_RAII_PRIORITY(handles_, INIT_PRIORITY_LOW){	\
			LOG_CIRCE_INFO("Registering interserver servlet: ", typeid(TOKEN_CAT2(InterserverServletWrapper_, __LINE__)::Message_).name());	\
			const AUTO(servlet_, ::boost::make_shared< ::Circe::Common::InterserverServletCallback>(&TOKEN_CAT2(InterserverServletWrapper_, __LINE__)::callback_));	\
			handles_.push(servlet_);	\
			static_cast<void>(insert_servlet_(TOKEN_CAT2(InterserverServletWrapper_, __LINE__)::Message_::ID, servlet_));	\
		}	\
	}	\
	/* The user is responsible for its definition, hence there is no brace after this prototype. */	\
	::Circe::Common::CbppResponse TOKEN_CAT2(InterserverServletWrapper_, __LINE__)::unwrapped_callback_(__VA_ARGS__)

#endif
