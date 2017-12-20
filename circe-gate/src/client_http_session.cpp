// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_session.hpp"
#include "singletons/client_http_acceptor.hpp"
#include "mmain.hpp"
#include <poseidon/http/request_headers.hpp>
#include <poseidon/http/response_headers.hpp>
#include <poseidon/http/exception.hpp>
#include <poseidon/zlib.hpp>

namespace Circe {
namespace Gate {

ClientHttpSession::ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket)
	: Poseidon::Http::Session(STD_MOVE(socket))
{
	LOG_CIRCE_DEBUG("ClientHttpSession constructor: remote = ", get_remote_info());
}
ClientHttpSession::~ClientHttpSession(){
	LOG_CIRCE_DEBUG("ClientHttpSession destructor: remote = ", get_remote_info());
	ClientHttpAcceptor::remove_session(this);
}

void ClientHttpSession::sync_authenticate(const Poseidon::Http::RequestHeaders &request_headers){
	PROFILE_ME;

	m_decoded_uri = "aa";
}

boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> ClientHttpSession::on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers){
	PROFILE_ME;

	return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
}

void ClientHttpSession::on_sync_expect(Poseidon::Http::RequestHeaders request_headers){
	PROFILE_ME;

	// Check authentication early.
	sync_authenticate(request_headers);
	LOG_CIRCE_DEBUG("Authenticated: remote = ", get_remote_info(), ", decoded_uri = ", m_decoded_uri, ", auth_token = ", m_auth_token);
	DEBUG_THROW_ASSERT(!m_decoded_uri.empty());

	return Poseidon::Http::Session::on_sync_expect(STD_MOVE(request_headers));
}
void ClientHttpSession::on_sync_request(Poseidon::Http::RequestHeaders request_headers, Poseidon::StreamBuffer request_entity){
	PROFILE_ME;

	// If no `Expect:` header was received, check authentication now.
	if(m_decoded_uri.empty()){
		sync_authenticate(request_headers);
		LOG_CIRCE_DEBUG("Authenticated: remote = ", get_remote_info(), ", decoded_uri = ", m_decoded_uri, ", auth_token = ", m_auth_token);
	}
	DEBUG_THROW_ASSERT(!m_decoded_uri.empty());

	const AUTO(response_encoding, Poseidon::Http::pick_content_encoding(request_headers));
	DEBUG_THROW_UNLESS(response_encoding != Poseidon::Http::CE_NOT_ACCEPTABLE, Poseidon::Http::Exception, Poseidon::Http::ST_NOT_ACCEPTABLE);

	// Read then clear these members.
	const AUTO(decoded_uri, STD_MOVE(m_decoded_uri));
	m_decoded_uri.clear();
	const AUTO(auth_token, STD_MOVE(m_auth_token));
	m_auth_token.clear();

	Poseidon::Http::ResponseHeaders response_headers = { 10001 };
	Poseidon::StreamBuffer response_entity;

	// Process the request. `HEAD` is handled in the same way as `GET`.
	// Fill in `response_headers.status_code` and `response_entity`.
	switch(request_headers.verb){
	case Poseidon::Http::V_HEAD:
	case Poseidon::Http::V_GET: {
		const AUTO(enable_http, get_config<bool>("protocol_enable_http", true));
		DEBUG_THROW_UNLESS(enable_http, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

		response_headers.status_code = Poseidon::Http::ST_OK;
		for(int i = 0; i < 1000; ++i){
			response_entity.put("meow ");
		}
		break; }

	case Poseidon::Http::V_POST: {
		const AUTO(enable_http, get_config<bool>("protocol_enable_http", true));
		DEBUG_THROW_UNLESS(enable_http, Poseidon::Http::Exception, Poseidon::Http::ST_SERVICE_UNAVAILABLE);

		response_headers.status_code = Poseidon::Http::ST_OK;
		break; }

	case Poseidon::Http::V_OPTIONS: {
		response_headers.status_code = Poseidon::Http::ST_OK;
		break; }

	default:
		DEBUG_THROW(Poseidon::Http::Exception, Poseidon::Http::ST_NOT_IMPLEMENTED);
	}
	// Fill in other fields.
	const AUTO(desc, Poseidon::Http::get_status_code_desc(response_headers.status_code));
	response_headers.reason = desc.desc_short;
	if(Poseidon::Http::is_keep_alive_enabled(request_headers) && (response_headers.status_code / 100 <= 3)){
		response_headers.headers.set(Poseidon::sslit("Connection"), "Keep-Alive");
	} else {
		response_headers.headers.set(Poseidon::sslit("Connection"), "Close");
	}
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Origin"), "*");
	response_headers.headers.set(Poseidon::sslit("Access-Control-Allow-Headers"), "Authorization, Content-Type");

	// Try compressing the response entity if it is not empty and no explicit encoding is given.
	const std::size_t size_original = response_entity.size();
	if((size_original != 0) && !response_headers.headers.has("Content-Encoding")){
		const AUTO(compression_level, get_config<int>("protocol_http_compression_level", 6));
		if(compression_level > 0){
			switch(response_encoding){
			case Poseidon::Http::CE_GZIP: {
				LOG_CIRCE_TRACE("Compressing HTTP response using GZIP: compression_level = ", compression_level);
				response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "gzip");
				Poseidon::Deflator deflator(true, compression_level);
				deflator.put(response_entity);
				response_entity = deflator.finalize();
				const std::size_t size_deflated = response_entity.size();
				LOG_CIRCE_TRACE("GZIP result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
				break; }
			case Poseidon::Http::CE_DEFLATE: {
				LOG_CIRCE_TRACE("Compressing HTTP response using DEFLATE: compression_level = ", compression_level);
				response_headers.headers.set(Poseidon::sslit("Content-Encoding"), "deflate");
				Poseidon::Deflator deflator(false, compression_level);
				deflator.put(response_entity);
				response_entity = deflator.finalize();
				const std::size_t size_deflated = response_entity.size();
				LOG_CIRCE_TRACE("DEFLATE result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), size_deflated * 100.0 / size_original, "%)");
				break; }
			default: {
				LOG_CIRCE_TRACE("The client did not send any `Accept-Encoding` we support.");
				break; }
			}
		}
	}
	if((response_headers.status_code / 100 >= 3) && response_entity.empty()){
		send_default(response_headers.status_code, STD_MOVE(response_headers.headers));
	} else {
		send(STD_MOVE(response_headers), STD_MOVE(response_entity));
	}
}

}
}
