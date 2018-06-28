// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_DEFINE_INTERSERVER_SERVLET_FOR_HPP_
#define CIRCE_COMMON_DEFINE_INTERSERVER_SERVLET_FOR_HPP_

#include "precompiled.hpp"
#include "interserver_servlet_callback.hpp"

#define CIRCE_DEFINE_INTERSERVER_SERVLET_FOR(how_, Msg_, conn_param_, msg_param_)	\
	namespace {	\
		struct POSEIDON_LAZY(POSEIDON_CAT2, Interserver_servlet_, __LINE__) {	\
			/* Declare the user-defined callback. */	\
			static ::Circe::Common::Interserver_response callback_(const ::boost::shared_ptr< ::Circe::Common::Interserver_connection> &, Msg_);	\
			/* This is the callback that matches `Interserver_servlet_callback`. */	\
			static ::Circe::Common::Interserver_response wrapper_(const ::boost::shared_ptr< ::Circe::Common::Interserver_connection> &conn_, ::boost::uint16_t message_id_, ::Poseidon::Stream_buffer payload_){	\
				POSEIDON_PROFILE_ME;	\
				POSEIDON_THROW_ASSERT(message_id_ == Msg_::id);	\
				Msg_ msg_;	\
				msg_.deserialize(payload_);	\
				CIRCE_LOG_TRACE("Received request from ", conn_->get_remote_info(), ": ", msg_);	\
				return callback_(conn_, STD_MOVE(msg_));	\
			}	\
		};	\
		/* Register the wrapped callback upon module load. */	\
		POSEIDON_MODULE_RAII_PRIORITY(handles_, ::Poseidon::module_init_priority_low){	\
			CIRCE_LOG_INFO("Registering interserver servlet: ", #Msg_);	\
			const AUTO(servlet_, ::boost::make_shared< ::Circe::Common::Interserver_servlet_callback>(& POSEIDON_LAZY(POSEIDON_CAT2, Interserver_servlet_, __LINE__)::wrapper_));	\
			handles_.push(servlet_);	\
			static_cast<void>(how_(Msg_::id, servlet_));	\
		}	\
	}	\
	/* The user is responsible for its definition, hence there is no brace after this prototype. */	\
	::Circe::Common::Interserver_response POSEIDON_LAZY(POSEIDON_CAT2, Interserver_servlet_, __LINE__)::callback_(const ::boost::shared_ptr< ::Circe::Common::Interserver_connection> & conn_param_, Msg_ msg_param_)

#endif
