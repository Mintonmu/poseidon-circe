// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "client_http_session.hpp"
#include "singletons/client_http_acceptor.hpp"

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

boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> ClientHttpSession::on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers){
	PROFILE_ME;

	return Poseidon::Http::Session::on_low_level_request_end(content_length, STD_MOVE(headers));
}

void ClientHttpSession::on_sync_expect(Poseidon::Http::RequestHeaders request_headers){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("Expect: ", request_headers.headers);
}
void ClientHttpSession::on_sync_request(Poseidon::Http::RequestHeaders request_headers, Poseidon::StreamBuffer request_entity){
	PROFILE_ME;

	LOG_POSEIDON_FATAL("Request: ", request_headers.headers, "\n  ", request_entity);
}

}
}
