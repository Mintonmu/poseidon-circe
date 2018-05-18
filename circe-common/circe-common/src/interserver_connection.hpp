// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_INTERSERVER_CONNECTION_HPP_
#define CIRCE_COMMON_INTERSERVER_CONNECTION_HPP_

#include <poseidon/fwd.hpp>
#include <poseidon/cbpp/fwd.hpp>
#include <poseidon/virtual_shared_from_this.hpp>
#include <poseidon/recursive_mutex.hpp>
#include <poseidon/uuid.hpp>
#include <poseidon/ip_port.hpp>
#include <poseidon/stream_buffer.hpp>
#include <poseidon/promise.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/array.hpp>
#include <boost/scoped_ptr.hpp>
#include "interserver_response.hpp"
#include "poseidon/option_map.hpp"

extern template class Poseidon::Promise_container<Circe::Common::Interserver_response>;

namespace Circe {
namespace Common {

typedef Poseidon::Promise_container<Interserver_response> Promised_response;

class Interserver_connection : public virtual Poseidon::Virtual_shared_from_this {
private:
	class Stream_cipher;
	class Message_filter;
	class Request_message_job;

public:
	enum {
		message_id_min  = 0x0001,
		message_id_max  = 0x0FFF,
	};
	BOOST_STATIC_ASSERT((message_id_max & (message_id_max + 1)) == 0);

public:
	static CONSTEXPR bool is_message_id_valid(boost::uint64_t message_id){
		return (message_id_min <= message_id) && (message_id <= message_id_max);
	}

	static std::size_t get_max_message_size();
	static int get_compression_level();
	static boost::uint64_t get_hello_timeout();

private:
	const std::string m_application_key;

	// This is only used by the workhorse thread.
	boost::scoped_ptr<Message_filter> m_message_filter;

	volatile bool m_authenticated;

	// These are protected by a mutex and can be accessed by any thread.
	mutable Poseidon::Recursive_mutex m_mutex;
	Poseidon::Uuid m_connection_uuid;
	boost::uint64_t m_timestamp;
	Poseidon::Option_map m_options;
	boost::uint64_t m_next_serial;
	boost::container::flat_multimap<boost::uint64_t, boost::weak_ptr<Promised_response> > m_weak_promises;

public:
	explicit Interserver_connection(const std::string &application_key);
	~Interserver_connection() OVERRIDE;

private:
	bool is_connection_uuid_set() const NOEXCEPT;
	void server_accept_hello(const Poseidon::Uuid &connection_uuid, boost::uint64_t timestamp, Poseidon::Option_map options_resp);
	void client_accept_hello(Poseidon::Option_map options_resp);
	void send_response(boost::uint64_t serial, Interserver_response resp);

	Message_filter *require_message_filter();
	void launch_inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::Stream_buffer deflated_payload);
	void inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::Stream_buffer &deflated_payload);
	void launch_deflate_and_send(boost::uint16_t magic_number, Poseidon::Stream_buffer magic_payload);
	void deflate_and_send(boost::uint16_t magic_number, Poseidon::Stream_buffer &magic_payload);

protected:
	virtual const Poseidon::Ip_port &layer5_get_remote_info() const NOEXCEPT = 0;
	virtual const Poseidon::Ip_port &layer5_get_local_info() const NOEXCEPT = 0;
	virtual bool layer5_has_been_shutdown() const NOEXCEPT = 0;
	virtual bool layer5_shutdown(long err_code, const char *err_msg) NOEXCEPT = 0;
	virtual void layer4_force_shutdown() NOEXCEPT = 0;
	virtual void layer5_send_data(boost::uint16_t magic_number, Poseidon::Stream_buffer deflated_payload) = 0;
	virtual void layer5_send_control(long status_code, Poseidon::Stream_buffer param) = 0;

	void layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::Stream_buffer deflated_payload);
	void layer5_on_receive_control(long status_code, Poseidon::Stream_buffer param);
	void layer4_on_close();

	// The client shall call this function before sending anything else.
	// The server shall not call this function.
	void layer7_client_say_hello(Poseidon::Option_map options_req);

	virtual void layer7_post_set_connection_uuid() = 0;
	virtual Interserver_response layer7_on_sync_message(boost::uint16_t message_id, Poseidon::Stream_buffer payload) = 0;

public:
	bool has_authenticated() const;
	// These functions throw an exception if the connection UUID has not been set.
	const Poseidon::Uuid &get_connection_uuid() const;
	const std::string &get_option(const char *key) const;

	const Poseidon::Ip_port &get_remote_info() const NOEXCEPT {
		return layer5_get_remote_info();
	}
	const Poseidon::Ip_port &get_local_info() const NOEXCEPT {
		return layer5_get_local_info();
	}
	bool has_been_shutdown() const NOEXCEPT {
		return layer5_has_been_shutdown();
	}
	bool shutdown(long err_code, const char *err_msg = "") NOEXCEPT {
		return layer5_shutdown(err_code, err_msg);
	}
	void force_shutdown() NOEXCEPT {
		return layer4_force_shutdown();
	}

	boost::shared_ptr<const Promised_response> send_request(const Poseidon::Cbpp::Message_base &msg);
	void send_notification(const Poseidon::Cbpp::Message_base &msg);
};

extern void wait_for_response(Poseidon::Cbpp::Message_base &msg, const boost::shared_ptr<const Promised_response> &promise);

}
}

#endif
