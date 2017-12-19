// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_HTTP_SESSION_HPP_
#define CIRCE_GATE_CLIENT_HTTP_SESSION_HPP_

#include <poseidon/http/session.hpp>
#include <poseidon/uuid.hpp>

namespace Circe {
namespace Gate {

class ClientWebSocketSession;

class ClientHttpSession : public Poseidon::Http::Session {
private:
	class WebSocketHandshakeJob;

private:
	const Poseidon::Uuid m_session_uuid;

public:
	explicit ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket);
	~ClientHttpSession() OVERRIDE;

protected:
	boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers) OVERRIDE;

	void on_sync_expect(Poseidon::Http::RequestHeaders request_headers) OVERRIDE;
	void on_sync_request(Poseidon::Http::RequestHeaders request_headers, Poseidon::StreamBuffer request_entity) OVERRIDE;

public:
	const Poseidon::Uuid &get_session_uuid() const {
		return m_session_uuid;
	}
};

}
}

#endif
