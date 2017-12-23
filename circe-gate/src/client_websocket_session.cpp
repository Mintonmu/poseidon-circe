// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_websocket_session.hpp"
#include "client_http_session.hpp"
#include "singletons/auth_connector.hpp"
#include "singletons/foyer_connector.hpp"
#include "common/cbpp_response.hpp"
#include "protocol/error_codes.hpp"
#include "protocol/messages_gate_auth.hpp"
#include "protocol/messages_gate_foyer.hpp"
#include "protocol/utilities.hpp"
#include <poseidon/websocket/exception.hpp>

namespace Circe {
namespace Gate {

ClientWebSocketSession::ClientWebSocketSession(const boost::shared_ptr<ClientHttpSession> &parent)
	: Poseidon::WebSocket::Session(parent)
	, m_session_uuid(parent->get_session_uuid())
{
	LOG_CIRCE_DEBUG("ClientWebSocketSession constructor: remote = ", get_remote_info());
}
ClientWebSocketSession::~ClientWebSocketSession(){
	LOG_CIRCE_DEBUG("ClientWebSocketSession destructor: remote = ", get_remote_info());
}

std::string ClientWebSocketSession::sync_authenticate(const std::string &decoded_uri, const Poseidon::OptionalMap &params){
	PROFILE_ME;

	const AUTO(auth_conn, AuthConnector::get_connection());
	DEBUG_THROW_UNLESS(auth_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY);
	const AUTO(foyer_conn, FoyerConnector::get_connection());
	DEBUG_THROW_UNLESS(foyer_conn, Poseidon::WebSocket::Exception, Poseidon::WebSocket::ST_GOING_AWAY);

	Protocol::AG_WebSocketAuthenticationResponse auth_resp;
	{
		Protocol::GA_WebSocketAuthenticationRequest auth_req;
		auth_req.client_uuid = get_session_uuid();
		auth_req.client_ip   = get_remote_info().ip();
		auth_req.decoded_uri = decoded_uri;
		Protocol::copy_key_values(auth_req.params, params);
		LOG_CIRCE_TRACE("Sending request: ", auth_req);
		Common::wait_for_response(auth_resp, auth_conn->send_request(auth_req));
		LOG_CIRCE_TRACE("Received response: ", auth_resp);
	}
	if(auth_resp.status_code != 0){
		DEBUG_THROW(Poseidon::WebSocket::Exception, auth_resp.status_code, Poseidon::SharedNts(auth_resp.message));
	}
/*
	Protocol::FG_ReturnWebSocketEstablishmentResult foyer_resp;
	{
		Protocol::GF_EstablishWebSocketConnection foyer_req;
		foyer_req.client_uuid = get_session_uuid();
		foyer_req.client_ip   = get_remote_info().ip();
		foyer_req.auth_token  = auth_resp.auth_token;
		foyer_req.decoded_uri = decoded_uri;
		Protocol::copy_key_values(foyer_req.params, params);
		LOG_CIRCE_TRACE("Sending request: ", foyer_req);
		Common::wait_for_response(foyer_resp, foyer_conn->send_request(foyer_req));
		LOG_CIRCE_TRACE("Received response: ", foyer_resp);
	}
	if(foyer_resp.status_code != 0){
		DEBUG_THROW(Poseidon::WebSocket::Exception, foyer_resp.status_code, Poseidon::SharedNts(foyer_resp.message));
	}
*/
	LOG_CIRCE_DEBUG("Established WebSocketConnection: remote = ", get_remote_info(), ", auth_token = ", auth_resp.auth_token);
	return STD_MOVE(auth_resp.auth_token);
}

void ClientWebSocketSession::on_sync_data_message(Poseidon::WebSocket::OpCode opcode, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("DATA: ", opcode, ": ", payload);
	send(Poseidon::WebSocket::OP_DATA_TEXT, Poseidon::StreamBuffer("hello world!"));
}

}
}
