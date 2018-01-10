// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_GATE_CLIENT_HTTP_SESSION_HPP_
#define CIRCE_GATE_CLIENT_HTTP_SESSION_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/http/session.hpp>
#include <poseidon/uuid.hpp>
#include <poseidon/websocket/status_codes.hpp>
#include <boost/optional.hpp>

namespace Circe {
namespace Gate {

class ClientWebSocketSession;

class ClientHttpSession : public Poseidon::Http::Session {
	friend ClientWebSocketSession;

private:
	class WebSocketHandshakeJob;

private:
	const Poseidon::Uuid m_client_uuid;

	// These are accessed only by the epoll thread.
	bool m_first_request;

	// These are accessed only by the primary thread.
	boost::optional<std::string> m_decoded_uri;
	boost::optional<std::string> m_auth_token;

public:
	explicit ClientHttpSession(Poseidon::Move<Poseidon::UniqueFile> socket);
	~ClientHttpSession() OVERRIDE;

private:
	void sync_decode_uri(const std::string &uri);
	void sync_authenticate(Poseidon::Http::Verb verb, const std::string &decoded_uri, const Poseidon::OptionalMap &params, const Poseidon::OptionalMap &headers);

	Poseidon::OptionalMap make_retry_after_headers(boost::uint64_t time_remaining) const;

protected:
	// Callbacks run in the epoll thread.
	boost::shared_ptr<Poseidon::Http::UpgradedSessionBase> on_low_level_request_end(boost::uint64_t content_length, Poseidon::OptionalMap headers) OVERRIDE;

	// Callbacks run in the primary thread.
	void on_sync_expect(Poseidon::Http::RequestHeaders req_headers) OVERRIDE;
	void on_sync_request(Poseidon::Http::RequestHeaders req_headers, Poseidon::StreamBuffer req_entity) OVERRIDE;

public:
	const Poseidon::Uuid &get_client_uuid() const {
		return m_client_uuid;
	}

	bool has_been_shutdown() const NOEXCEPT;
	bool shutdown(Poseidon::WebSocket::StatusCode status_code = Poseidon::WebSocket::ST_INTERNAL_ERROR, const char *reason = "") NOEXCEPT;

	bool send(Poseidon::Http::ResponseHeaders response_headers, Poseidon::StreamBuffer entity);
	bool send_default_and_shutdown(Poseidon::Http::StatusCode status_code, const Poseidon::OptionalMap &headers = Poseidon::OptionalMap()) NOEXCEPT;
};

}
}

#endif
