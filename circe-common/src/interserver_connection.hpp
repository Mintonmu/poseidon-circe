// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTION_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTION_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/mutex.hpp>
#include <poseidon/uuid.hpp>
#include <poseidon/ip_port.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/promise.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/array.hpp>

namespace Circe {
namespace Common {

class CbppResponse;

class InterserverConnection : public virtual Poseidon::VirtualSharedFromThis {
public:
	enum {
		MESSAGE_ID_MIN  = 0x0001,
		MESSAGE_ID_MAX  = 0x0FFF,
	};

	class RequestMessageJob;

	typedef Poseidon::PromiseContainer<CbppResponse> PromisedResponse;

private:
	static void inflate_and_dispatch(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &deflated_payload);
	static void deflate_and_send(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &magic_payload);

public:
	static CONSTEXPR bool is_message_id_valid(boost::uint64_t message_id){
		return (MESSAGE_ID_MIN <= message_id) && (message_id <= MESSAGE_ID_MAX);
	}

private:
	const std::string m_application_key;

	// These are protected by a mutex and can be accessed by any thread.
	mutable Poseidon::Mutex m_mutex;
	Poseidon::Uuid m_connection_uuid;
	boost::array<unsigned char, 12> m_nonce;
	boost::uint32_t m_next_serial;
	boost::container::flat_multimap<boost::uint32_t, boost::weak_ptr<PromisedResponse> > m_weak_promises;

	// These are only used by the workhorse thread.
	boost::scoped_ptr<Poseidon::Inflator> m_inflator;
	boost::scoped_ptr<Poseidon::Deflator> m_deflator;

public:
	explicit InterserverConnection(std::string application_key);
	~InterserverConnection() OVERRIDE;

private:
	boost::array<unsigned char, 12> create_nonce() const;
	boost::array<unsigned char, 32> calculate_checksum(const Poseidon::Uuid &connection_uuid, const boost::array<unsigned char, 12> &nonce, bool response) const;

	bool is_connection_uuid_set() const NOEXCEPT;
	void server_accept_hello(const Poseidon::Uuid &connection_uuid, const boost::array<unsigned char, 12> &nonce);

	void launch_inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload);
	void launch_deflate_and_send(boost::uint16_t magic_number, Poseidon::StreamBuffer magic_payload);

	void sync_dispatch_message(boost::uint16_t message_id, bool send_response, boost::uint32_t serial, Poseidon::StreamBuffer payload);

protected:
	virtual const Poseidon::IpPort &layer5_get_remote_info() const NOEXCEPT = 0;
	virtual const Poseidon::IpPort &layer5_get_local_info() const NOEXCEPT = 0;
	virtual bool layer5_has_been_shutdown() const NOEXCEPT = 0;
	virtual bool layer5_shutdown() NOEXCEPT = 0;
	virtual void layer4_force_shutdown() NOEXCEPT = 0;
	virtual void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload) = 0;
	virtual void layer5_send_control(long status_code, Poseidon::StreamBuffer param) = 0;

	void layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload);
	void layer5_on_receive_control(long status_code, Poseidon::StreamBuffer param);
	void layer4_on_close();

	// The client shall call this function before sending anything else.
	// The server shall not call this function.
	void layer7_client_say_hello();

	virtual CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) = 0;

public:
	// This function throws an exception if the connection UUID has not been set.
	const Poseidon::Uuid &get_connection_uuid() const;

	const Poseidon::IpPort &get_remote_info() const NOEXCEPT {
		return layer5_get_remote_info();
	}
	const Poseidon::IpPort &get_local_info() const NOEXCEPT {
		return layer5_get_local_info();
	}
	bool has_been_shutdown() const NOEXCEPT {
		return layer5_has_been_shutdown();
	}
	void force_shutdown() NOEXCEPT {
		return layer4_force_shutdown();
	}

	void send(const boost::shared_ptr<PromisedResponse> &promise, boost::uint16_t message_id, Poseidon::StreamBuffer payload);
	void send(const boost::shared_ptr<PromisedResponse> &promise, const Poseidon::Cbpp::MessageBase &msg);
	bool shutdown(long err_code, const char *err_msg = "") NOEXCEPT;
};

}
}

#endif
