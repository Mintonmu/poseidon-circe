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
private:
	class MessageFilter;
	class RequestMessageJob;

public:
	enum {
		MESSAGE_ID_MIN  = 0x0001,
		MESSAGE_ID_MAX  = 0x0FFF,
	};
	BOOST_STATIC_ASSERT((MESSAGE_ID_MAX & (MESSAGE_ID_MAX + 1)) == 0);

	typedef Poseidon::PromiseContainer<CbppResponse> PromisedResponse;

private:
	static void inflate_and_dispatch(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &deflated_payload);
	static void deflate_and_send(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &magic_payload);

public:
	static CONSTEXPR bool is_message_id_valid(boost::uint64_t message_id){
		return (MESSAGE_ID_MIN <= message_id) && (message_id <= MESSAGE_ID_MAX);
	}

	static std::size_t get_max_message_size();
	static int get_compression_level();
	static boost::uint64_t get_hello_timeout();

private:
	const std::string m_application_key;

	// This is only used by the workhorse thread.
	boost::scoped_ptr<MessageFilter> m_message_filter;

	// These are protected by a mutex and can be accessed by any thread.
	mutable Poseidon::Mutex m_mutex;
	Poseidon::Uuid m_connection_uuid;
	boost::uint64_t m_timestamp;
	boost::uint64_t m_next_serial;
	boost::container::flat_multimap<boost::uint64_t, boost::weak_ptr<PromisedResponse> > m_weak_promises;

public:
	explicit InterserverConnection(const std::string &application_key);
	~InterserverConnection() OVERRIDE;

private:
	boost::array<unsigned char, 32> calculate_checksum(bool response, const Poseidon::Uuid &connection_uuid, boost::uint64_t timestamp) const;

	bool is_connection_uuid_set() const NOEXCEPT;
	void server_accept_hello(const Poseidon::Uuid &connection_uuid, boost::uint64_t timestamp);

	void launch_inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::StreamBuffer deflated_payload);
	void launch_deflate_and_send(boost::uint16_t magic_number, Poseidon::StreamBuffer magic_payload);

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

	virtual void layer7_post_set_connection_uuid() = 0;
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
