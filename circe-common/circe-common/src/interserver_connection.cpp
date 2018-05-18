// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connection.hpp"
#include "interserver_response.hpp"
#include "protocol/exception.hpp"
#include "protocol/utilities.hpp"
#include "mmain.hpp"
#include <poseidon/tiny_exception.hpp>
#include <poseidon/socket_base.hpp>
#include <poseidon/sha256.hpp>
#include <poseidon/zlib.hpp>
#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/singletons/workhorse_camp.hpp>
#include <poseidon/job_base.hpp>
#include <openssl/aes.h>

template class Poseidon::Promise_container<Circe::Common::Interserver_response>;

namespace Circe {
namespace Common {

namespace {

// Magic Flags
enum {
	magic_flag_is_response    = 0x8000,
	magic_flag_wants_response = 0x4000,
	magic_flag_predefined     = 0x2000,
	magic_flag_reserved       = 0x1000,
};

// Predefined Messages
#define CBPP_MESSAGE_EMIT_EXTERNAL_DEFINITIONS  1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#define MESSAGE_NAME   Client_hello
#define MESSAGE_ID     magic_flag_predefined + 1
#define MESSAGE_FIELDS \
	FIELD_FIXED        (connection_uuid, 16)	\
	FIELD_VUINT        (timestamp)	\
	FIELD_FIXED        (checksum_req, 32)	\
	FIELD_ARRAY        (options_req,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   Server_hello
#define MESSAGE_ID     magic_flag_predefined + 2
#define MESSAGE_FIELDS \
	FIELD_FIXED        (checksum_resp, 32)	\
	FIELD_ARRAY        (options_resp,	\
	  FIELD_STRING       (key)	\
	  FIELD_STRING       (value)	\
	)	\
	//
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   User_request_header
#define MESSAGE_ID     0
#define MESSAGE_FIELDS \
	FIELD_VUINT        (serial)	\
	// payload
#include <poseidon/cbpp/message_generator.inl>

#define MESSAGE_NAME   User_response_header
#define MESSAGE_ID     0
#define MESSAGE_FIELDS \
	FIELD_VUINT        (serial)	\
	FIELD_VINT         (err_code)	\
	FIELD_STRING       (err_msg)	\
	// payload
#include <poseidon/cbpp/message_generator.inl>

#pragma GCC diagnostic pop
#undef CBPP_MESSAGE_EMIT_EXTERNAL_DEFINITIONS

// SHA-256 Salt strings
#define SALT_AUTHENTICATION    "G_AUTH"
#define SALT_NORMAL_DATA       "G_DATA"
#define SALT_CLIENT_HELLO      "C_HELLO"
#define SALT_SERVER_HELLO      "S_HELLO"

Poseidon::Sha256 calculate_checksum(const std::string &application_key, const char *salt, const Poseidon::Uuid &connection_uuid, boost::uint64_t timestamp){
	PROFILE_ME;

	Poseidon::Sha256_ostream sha256_os;
	// Write the application key.
	sha256_os.write(application_key.data(), static_cast<std::streamsize>(application_key.size()));
	sha256_os.put(0);
	// Write the salt.
	sha256_os.write(salt, static_cast<std::streamsize>(std::strlen(salt)));
	sha256_os.put(0);
	// Write the connection UUID.
	sha256_os.write(reinterpret_cast<const char *>(connection_uuid.data()), 16);
	sha256_os.put(0);
	// Write the timestamp in big-endian order.
	boost::uint64_t timestamp_be;
	Poseidon::store_be(timestamp_be, timestamp);
	sha256_os.write(reinterpret_cast<const char *>(&timestamp_be), 8);
	sha256_os.put(0);
	return sha256_os.finalize();
}

}

class Interserver_connection::Stream_cipher : NONCOPYABLE {
private:
	::AES_KEY m_key;
	boost::array<unsigned char, 16> m_chunk;
	unsigned m_count;

public:
	explicit Stream_cipher(const boost::array<unsigned char, 32> &seed){
		reset(seed);
	}

public:
	void reset(const boost::array<unsigned char, 32> &seed){
		DEBUG_THROW_ASSERT(::AES_set_encrypt_key(seed.data(), 256, &m_key) == 0);
		m_chunk.fill(42);
		m_count = 16;
	}
	int filter(int input, bool to_encrypt){
		if(m_count == 16){
			::AES_encrypt(m_chunk.data(), m_chunk.data(), &m_key);
			m_count = 0;
		}
		unsigned char &mask = m_chunk.at(m_count++);
		// https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Cipher__feedback_(CFB)
		//   Encryption:
		//     ciphertext <= mask ^ plaintext
		//     mask       <= ENCRYPT(ciphertext)
		//   Decryption:
		//     plaintext  <= mask ^ ciphertext
		//     mask       <= ENCRYPT(ciphertext)
		const int output = mask ^ input;
		mask = static_cast<unsigned char>(to_encrypt ? output : input);
		return output;
	}
};

class Interserver_connection::Message_filter : NONCOPYABLE {
private:
	// Decoder
	Stream_cipher m_decryptor;
	Poseidon::Inflator m_inflator;
	// Encoder
	Stream_cipher m_encryptor;
	Poseidon::Deflator m_deflator;

public:
	Message_filter(const boost::array<unsigned char, 32> &seed, int compression_level)
		: m_decryptor(seed), m_inflator(false)
		, m_encryptor(seed), m_deflator(false, compression_level)
	{
		//
	}
	~Message_filter(){
		// Silence the warnings.
		m_inflator.clear();
		m_deflator.clear();
	}

public:
	Poseidon::Stream_buffer decode(Poseidon::Stream_buffer encoded_payload){
		PROFILE_ME;

		Poseidon::Stream_buffer temp;
		// Step 1: Decrypt the block.
		for(std::size_t i = encoded_payload.size(); i != 0; --i){
			temp.put(m_decryptor.filter(encoded_payload.get(), false));
		}
		// Step 2: Append the terminator bytes to this block.
		temp.put(0x00);
		temp.put(0x00);
		temp.put(0xFF);
		temp.put(0xFF);
		// Step 3: Inflate the block.
		m_inflator.put(temp);
		m_inflator.flush();
		// Step 4: Return the block decoded.
		Poseidon::Stream_buffer magic_payload;
		magic_payload.swap(m_inflator.get_buffer());
		return magic_payload;
	}
	void reseed_decoder_prng(const boost::array<unsigned char, 32> &seed){
		PROFILE_ME;

		m_decryptor.reset(seed);
	}
	Poseidon::Stream_buffer encode(Poseidon::Stream_buffer magic_payload){
		PROFILE_ME;

		Poseidon::Stream_buffer temp;
		// Step 1: Deflate the block.
		m_deflator.put(magic_payload);
		m_deflator.flush();
		temp.swap(m_deflator.get_buffer());
		// Step 2: Drop the the terminator bytes from this block.
		DEBUG_THROW_ASSERT(temp.unput() == 0xFF);
		DEBUG_THROW_ASSERT(temp.unput() == 0xFF);
		DEBUG_THROW_ASSERT(temp.unput() == 0x00);
		DEBUG_THROW_ASSERT(temp.unput() == 0x00);
		// Step 3: Encrypt the block.
		Poseidon::Stream_buffer encoded_payload;
		for(std::size_t i = temp.size(); i != 0; --i){
			encoded_payload.put(m_encryptor.filter(temp.get(), true));
		}
		// Step 4: Return the block encoded.
		return encoded_payload;
	}
	void reseed_encoder_prng(const boost::array<unsigned char, 32> &seed){
		PROFILE_ME;

		m_encryptor.reset(seed);
	}
};

class Interserver_connection::Request_message_job : public Poseidon::Job_base {
private:
	const Poseidon::Socket_base::Delayed_shutdown_guard m_guard;
	const boost::weak_ptr<Interserver_connection> m_weak_connection;

	boost::uint16_t m_message_id;
	bool m_send_response;
	boost::uint64_t m_serial;
	Poseidon::Stream_buffer m_payload;

public:
	Request_message_job(const boost::shared_ptr<Interserver_connection> &connection, boost::uint16_t message_id, bool send_response, boost::uint64_t serial, Poseidon::Stream_buffer payload)
		: m_guard(boost::dynamic_pointer_cast<Poseidon::Socket_base>(connection)), m_weak_connection(connection)
		, m_message_id(message_id), m_send_response(send_response), m_serial(serial), m_payload(STD_MOVE(payload))
	{
		//
	}

protected:
	boost::weak_ptr<const void> get_category() const FINAL {
		// Interserver messages are context-free.
		return VAL_INIT;
	}
	void perform() FINAL {
		PROFILE_ME;

		const AUTO(connection, m_weak_connection.lock());
		if(!connection){
			return;
		}

		try {
			LOG_CIRCE_TRACE("Dispatching message: message_id = ", m_message_id);
			Interserver_response resp;
			try {
				resp = connection->layer7_on_sync_message(m_message_id, STD_MOVE(m_payload));
			} catch(Protocol::Exception &e){
				LOG_CIRCE_ERROR("Protocol::Exception thrown: mesasge_id = ", m_message_id, ", err_code = ", e.get_status_code(), ", err_msg = ", e.what());
				resp = Interserver_response(e.get_status_code(), e.what());
			} catch(std::exception &e){
				LOG_CIRCE_ERROR("std::exception thrown: mesasge_id = ", m_message_id, ", err_msg = ", e.what());
				resp = Interserver_response(Protocol::error_internal_error, e.what());
			}
			const long status_code = resp.get_err_code();
			if(m_send_response){
				connection->send_response(m_serial, STD_MOVE(resp));
			}
			if(status_code < 0){
				char str[256];
				std::sprintf(str, "Fatal error %ld", status_code);
				connection->shutdown(Protocol::error_internal_error, str);
			}
			LOG_CIRCE_TRACE("Done dispatching message: message_id = ", m_message_id);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
			connection->shutdown(Protocol::error_internal_error, e.what());
		}
	}
};

std::size_t Interserver_connection::get_max_message_size(){
	return get_config<std::size_t>("interserver_max_message_size", 1048576);
}
int Interserver_connection::get_compression_level(){
	return get_config<int>("interserver_compression_level", 6);
}
boost::uint64_t Interserver_connection::get_hello_timeout(){
	return get_config<boost::uint64_t>("interserver_hello_timeout", 60000);
}

Interserver_connection::Interserver_connection(const std::string &application_key)
	: m_application_key(application_key)
	, m_authenticated(false), m_connection_uuid(), m_next_serial(0)
{
	LOG_CIRCE_INFO("Interserver_connection constructor: this = ", (void *)this);
}
Interserver_connection::~Interserver_connection(){
	LOG_CIRCE_INFO("Interserver_connection destructor: this = ", (void *)this);
}

bool Interserver_connection::is_connection_uuid_set() const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	return !!m_connection_uuid;
}
void Interserver_connection::server_accept_hello(const Poseidon::Uuid &connection_uuid, boost::uint64_t timestamp, Poseidon::Option_map options_resp){
	PROFILE_ME;
	LOG_CIRCE_INFO("Accepted client HELLO from ", get_remote_info(), " with options ", options_resp);

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_connection_uuid, Poseidon::Exception, Poseidon::Rcnts::view("server_accept_hello() shall be called exactly once by the server and must not be called by the client"));
	const AUTO(checksum_resp, calculate_checksum(m_application_key, SALT_SERVER_HELLO, connection_uuid, timestamp));
	{
		Server_hello msg;
		msg.checksum_resp = checksum_resp;
		Protocol::copy_key_values(msg.options_resp, options_resp);
		LOG_CIRCE_TRACE("Sending server HELLO: remote = ", get_remote_info(), ", msg = ", msg);
		launch_deflate_and_send(boost::numeric_cast<boost::uint16_t>(msg.get_id()), msg);
	}
	m_connection_uuid = connection_uuid;
	m_timestamp       = timestamp;
	m_options         = STD_MOVE(options_resp);
	layer7_post_set_connection_uuid();
}
void Interserver_connection::client_accept_hello(Poseidon::Option_map options_resp){
	PROFILE_ME;
	LOG_CIRCE_INFO("Accepted server HELLO from ", get_remote_info(), " with options ", options_resp);

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(m_connection_uuid, Poseidon::Exception, Poseidon::Rcnts::view("client_accept_hello() shall be called exactly once by the client and must not be called by the server"));
	m_options         = STD_MOVE(options_resp);
	layer7_post_set_connection_uuid();
}
void Interserver_connection::send_response(boost::uint64_t serial, Interserver_response resp){
	PROFILE_ME;

	const boost::uint64_t message_id = resp.get_message_id();
	DEBUG_THROW_UNLESS((message_id == 0) || is_message_id_valid(resp.get_message_id()), Poseidon::Exception, Poseidon::Rcnts::view("message_id out of range"));

	boost::uint16_t magic_number = boost::numeric_cast<boost::uint16_t>(message_id);
	Poseidon::add_flags(magic_number, magic_flag_is_response);
	// Initialize the header.
	User_response_header hdr;
	hdr.serial   = serial;
	hdr.err_code = resp.get_err_code();
	hdr.err_msg  = STD_MOVE(resp.get_err_msg());
	// Concatenate the payload to the header.
	Poseidon::Stream_buffer magic_payload;
	hdr.serialize(magic_payload);
	if(message_id != 0){
		magic_payload.splice(resp.get_payload());
	}
	// Send it.
	LOG_CIRCE_TRACE("Sending user-defined response: remote = ", get_remote_info(), ", hdr = ", hdr, ", message_id = ", resp.get_message_id(), ", payload_size = ", magic_payload.size());
	launch_deflate_and_send(magic_number, STD_MOVE(magic_payload));
}

Interserver_connection::Message_filter *Interserver_connection::require_message_filter(){
	PROFILE_ME;

	if(!m_message_filter){
		const AUTO(checksum_seed, calculate_checksum(m_application_key, SALT_AUTHENTICATION, Poseidon::Uuid(), 0));
		m_message_filter.reset(new Message_filter(checksum_seed, get_compression_level()));
	}
	return m_message_filter.get();
}
void Interserver_connection::launch_inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::Stream_buffer encoded_payload){
	PROFILE_ME;

	Poseidon::Workhorse_camp::enqueue(VAL_INIT,
		boost::bind(&Interserver_connection::inflate_and_dispatch, virtual_shared_from_this<Interserver_connection>(), magic_number, STD_MOVE_IDN(encoded_payload)),
		reinterpret_cast<std::size_t>(this));
}
void Interserver_connection::inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::Stream_buffer &encoded_payload)
try {
	PROFILE_ME;

	if(has_been_shutdown()){
		return;
	}
	LOG_CIRCE_TRACE("Inflate and dispatch: connection = ", (void *)this);

	switch(magic_number){
	case Client_hello::ID: {
		Client_hello msg(STD_MOVE(encoded_payload));
		LOG_CIRCE_TRACE("Received client HELLO: remote = ", get_remote_info(), ", msg = ", msg);
		const AUTO(checksum_req, calculate_checksum(m_application_key, SALT_CLIENT_HELLO, Poseidon::Uuid(msg.connection_uuid), msg.timestamp));
		CIRCE_PROTOCOL_THROW_UNLESS(msg.checksum_req == checksum_req, Protocol::error_authorization_failure, Poseidon::Rcnts::view("Request checksum failed verification"));
		server_accept_hello(Poseidon::Uuid(msg.connection_uuid), msg.timestamp, Protocol::copy_key_values(STD_MOVE_IDN(msg.options_req)));
		DEBUG_THROW_ASSERT(is_connection_uuid_set());
		const AUTO(checksum_seedx, calculate_checksum(m_application_key, SALT_NORMAL_DATA, m_connection_uuid, m_timestamp));
		require_message_filter()->reseed_decoder_prng(checksum_seedx);
		Poseidon::atomic_store(m_authenticated, true, Poseidon::memory_order_release);
		return; }
	case Server_hello::ID: {
		DEBUG_THROW_ASSERT(is_connection_uuid_set());
		Server_hello msg(STD_MOVE(encoded_payload));
		LOG_CIRCE_TRACE("Received server HELLO: remote = ", get_remote_info(), ", msg = ", msg);
		const AUTO(checksum_resp, calculate_checksum(m_application_key, SALT_SERVER_HELLO, m_connection_uuid, m_timestamp));
		CIRCE_PROTOCOL_THROW_UNLESS(msg.checksum_resp == checksum_resp, Protocol::error_authorization_failure, Poseidon::Rcnts::view("Response checksum failed verification"));
		client_accept_hello(Protocol::copy_key_values(STD_MOVE_IDN(msg.options_resp)));
		Poseidon::atomic_store(m_authenticated, true, Poseidon::memory_order_release);
		return; }
	}
	CIRCE_PROTOCOL_THROW_UNLESS(Poseidon::has_none_flags_of(magic_number, magic_flag_predefined | magic_flag_reserved), Protocol::error_invalid_argument, Poseidon::Rcnts::view("Reserved bits set"));

	const std::size_t size_deflated = encoded_payload.size();
	Poseidon::Stream_buffer magic_payload = require_message_filter()->decode(STD_MOVE(encoded_payload));
	const std::size_t size_original = magic_payload.size();
	LOG_CIRCE_TRACE("Inflate result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), (size_original != 0) ? size_deflated * 100.0l / size_original : 0.0l, "%)");

	boost::uint16_t message_id = magic_number & message_id_max;
	if(Poseidon::has_any_flags_of(magic_number, magic_flag_is_response)){
		User_response_header hdr;
		hdr.deserialize(magic_payload);
		LOG_CIRCE_TRACE("Received user-defined response: remote = ", get_remote_info(), ", hdr = ", hdr, ", message_id = ", message_id, ", payload_size = ", magic_payload.size());
		// Satisfy the promise.
		boost::shared_ptr<Promised_response> promise;
		{
			const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
			const AUTO(it, m_weak_promises.find(hdr.serial));
			if(it != m_weak_promises.end()){
				promise = it->second.lock();
				m_weak_promises.erase(it);
			}
		}
		if(promise){
			Interserver_response resp;
			resp.m_err_code   = hdr.err_code;
			resp.m_err_msg    = STD_MOVE(hdr.err_msg);
			resp.m_message_id = message_id;
			resp.m_payload    = STD_MOVE(magic_payload);
			promise->set_success(STD_MOVE(resp));
		}
	} else if(Poseidon::has_any_flags_of(magic_number, magic_flag_wants_response)){
		User_request_header hdr;
		hdr.deserialize(magic_payload);
		LOG_CIRCE_TRACE("Received user-defined request: remote = ", get_remote_info(), ", hdr = ", hdr, ", message_id = ", message_id, ", payload_size = ", magic_payload.size());
		Poseidon::enqueue(boost::make_shared<Request_message_job>(virtual_shared_from_this<Interserver_connection>(), message_id, true, hdr.serial, STD_MOVE(magic_payload)));
	} else {
		LOG_CIRCE_TRACE("Received user-defined notification: remote = ", get_remote_info(), ", message_id = ", message_id, ", payload_size = ", magic_payload.size());
		Poseidon::enqueue(boost::make_shared<Request_message_job>(virtual_shared_from_this<Interserver_connection>(), message_id, false, 0xDEADBEEF, STD_MOVE(magic_payload)));
	}
} catch(Protocol::Exception &e){
	LOG_CIRCE_ERROR("Protocol::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
	shutdown(e.get_status_code(), e.what());
} catch(std::exception &e){
	LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
	shutdown(Protocol::error_internal_error, e.what());
}
void Interserver_connection::launch_deflate_and_send(boost::uint16_t magic_number, Poseidon::Stream_buffer magic_payload){
	PROFILE_ME;

	Poseidon::Workhorse_camp::enqueue(VAL_INIT,
		boost::bind(&Interserver_connection::deflate_and_send, virtual_shared_from_this<Interserver_connection>(), magic_number, STD_MOVE_IDN(magic_payload)),
		reinterpret_cast<std::size_t>(this));
}
void Interserver_connection::deflate_and_send(boost::uint16_t magic_number, Poseidon::Stream_buffer &magic_payload)
try {
	PROFILE_ME;

	if(has_been_shutdown()){
		return;
	}
	LOG_CIRCE_TRACE("Deflate and send: connection = ", (void *)this);

	switch(magic_number){
	case Client_hello::ID: {
		DEBUG_THROW_ASSERT(is_connection_uuid_set());
		layer5_send_data(magic_number, STD_MOVE(magic_payload));
		const AUTO(checksum_seedx, calculate_checksum(m_application_key, SALT_NORMAL_DATA, m_connection_uuid, m_timestamp));
		require_message_filter()->reseed_encoder_prng(checksum_seedx);
		return; }
	case Server_hello::ID: {
		DEBUG_THROW_ASSERT(is_connection_uuid_set());
		layer5_send_data(magic_number, STD_MOVE(magic_payload));
		return; }
	}
	CIRCE_PROTOCOL_THROW_UNLESS(Poseidon::has_none_flags_of(magic_number, magic_flag_predefined | magic_flag_reserved), Protocol::error_invalid_argument, Poseidon::Rcnts::view("Reserved bits set"));

	const std::size_t size_original = magic_payload.size();
	Poseidon::Stream_buffer encoded_payload = require_message_filter()->encode(STD_MOVE(magic_payload));
	const std::size_t size_deflated = encoded_payload.size();
	LOG_CIRCE_TRACE("Deflate result: ", size_deflated, " / ", size_original, " (", std::fixed, std::setprecision(3), (size_original != 0) ? size_deflated * 100.0l / size_original : 0.0l, "%)");

	layer5_send_data(magic_number, STD_MOVE(encoded_payload));
} catch(Protocol::Exception &e){
	LOG_CIRCE_ERROR("Protocol::Exception thrown: status_code = ", e.get_status_code(), ", what = ", e.what());
	shutdown(e.get_status_code(), e.what());
} catch(std::exception &e){
	LOG_CIRCE_ERROR("std::exception thrown: what = ", e.what());
	shutdown(Protocol::error_internal_error, e.what());
}

void Interserver_connection::layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::Stream_buffer encoded_payload){
	PROFILE_ME;

	launch_inflate_and_dispatch(magic_number, STD_MOVE(encoded_payload));
}
void Interserver_connection::layer5_on_receive_control(long status_code, Poseidon::Stream_buffer param){
	PROFILE_ME;
	DEBUG_THROW_ASSERT(has_authenticated());

	switch(status_code){
	case Poseidon::Cbpp::status_shutdown:
		LOG_CIRCE_INFO("Received SHUTDOWN frame from ", get_remote_info());
		layer5_shutdown(Poseidon::Cbpp::status_shutdown, static_cast<const char *>(param.squash()));
		break;
	case Poseidon::Cbpp::status_ping:
		LOG_CIRCE_TRACE("Received PING frame from ", get_remote_info(), ": param = ", param);
		layer5_send_control(Poseidon::Cbpp::status_pong, STD_MOVE(param));
		break;
	case Poseidon::Cbpp::status_pong:
		LOG_CIRCE_TRACE("Received PONG frame from ", get_remote_info(), ": param = ", param);
		break;
	default:
		LOG_CIRCE_WARNING("Received CBPP error from ", get_remote_info(), ": status_code = ", status_code, ", param = ", param);
		break;
	}
}
void Interserver_connection::layer4_on_close(){
	PROFILE_ME;

	VALUE_TYPE(m_weak_promises) weak_promises;
	{
		const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
		DEBUG_THROW_ASSERT(has_been_shutdown());
		weak_promises.swap(m_weak_promises);
	}
	for(AUTO(it, weak_promises.begin()); it != weak_promises.end(); ++it){
		const AUTO(promise, it->second.lock());
		if(!promise){
			continue;
		}
		static const AUTO(s_exception, STD_MAKE_EXCEPTION_PTR(Poseidon::Tiny_exception("Connection was lost before a response could be received")));
		promise->set_exception(s_exception, false);
	}
}

void Interserver_connection::layer7_client_say_hello(Poseidon::Option_map options_req){
	PROFILE_ME;

	const AUTO(connection_uuid, Poseidon::Uuid::random());
	const AUTO(timestamp, Poseidon::get_utc_time());
	LOG_CIRCE_INFO("Sending client HELLO to ", get_remote_info(), " with options ", options_req);

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!m_connection_uuid, Poseidon::Exception, Poseidon::Rcnts::view("layer7_client_say_hello() shall be called exactly once by the client and must not be called by the server"));
	const AUTO(checksum_req, calculate_checksum(m_application_key, SALT_CLIENT_HELLO, connection_uuid, timestamp));
	{
		Client_hello msg;
		msg.connection_uuid = connection_uuid;
		msg.timestamp       = timestamp;
		msg.checksum_req    = checksum_req;
		Protocol::copy_key_values(msg.options_req, STD_MOVE(options_req));
		LOG_CIRCE_TRACE("Sending client HELLO: remote = ", get_remote_info(), ", msg = ", msg);
		launch_deflate_and_send(boost::numeric_cast<boost::uint16_t>(msg.get_id()), msg);
	}
	m_connection_uuid = connection_uuid;
	m_timestamp       = timestamp;
}

bool Interserver_connection::has_authenticated() const {
	return Poseidon::atomic_load(m_authenticated, Poseidon::memory_order_consume);
}
const Poseidon::Uuid &Interserver_connection::get_connection_uuid() const {
	DEBUG_THROW_UNLESS(is_connection_uuid_set(), Poseidon::Exception, Poseidon::Rcnts::view("Interserver_connection has not been fully established"));
	return m_connection_uuid;
}
const std::string &Interserver_connection::get_option(const char *key) const {
	DEBUG_THROW_UNLESS(is_connection_uuid_set(), Poseidon::Exception, Poseidon::Rcnts::view("Interserver_connection has not been fully established"));
	return m_options.get(key);
}

boost::shared_ptr<const Promised_response> Interserver_connection::send_request(const Poseidon::Cbpp::Message_base &msg){
	PROFILE_ME;

	LOG_CIRCE_TRACE("Sending user-defined request: remote = ", get_remote_info(), ", msg = ", msg);
	const boost::uint64_t message_id = msg.get_id();
	DEBUG_THROW_UNLESS(is_message_id_valid(message_id), Poseidon::Exception, Poseidon::Rcnts::view("message_id out of range"));

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::Rcnts::view("Interserver_connection was lost"));
	const boost::uint64_t serial = ++m_next_serial;
	AUTO(promise, boost::make_shared<Promised_response>());
	const AUTO(it, m_weak_promises.emplace(serial, promise));
	try {
		boost::uint16_t magic_number = boost::numeric_cast<boost::uint16_t>(message_id);
		Poseidon::add_flags(magic_number, magic_flag_wants_response);
		// Initialize the header.
		User_request_header hdr;
		hdr.serial   = serial;
		// Concatenate the payload to the header.
		Poseidon::Stream_buffer magic_payload;
		hdr.serialize(magic_payload);
		msg.serialize(magic_payload);
		launch_deflate_and_send(magic_number, STD_MOVE(magic_payload));
	} catch(...){
		m_weak_promises.erase(it);
		throw;
	}
	return STD_MOVE_IDN(promise);
}
void Interserver_connection::send_notification(const Poseidon::Cbpp::Message_base &msg){
	PROFILE_ME;

	LOG_CIRCE_TRACE("Sending user-defined notification: remote = ", get_remote_info(), ", msg = ", msg);
	const boost::uint64_t message_id = msg.get_id();
	DEBUG_THROW_UNLESS(is_message_id_valid(message_id), Poseidon::Exception, Poseidon::Rcnts::view("message_id out of range"));

	const Poseidon::Recursive_mutex::Unique_lock lock(m_mutex);
	DEBUG_THROW_UNLESS(!has_been_shutdown(), Poseidon::Exception, Poseidon::Rcnts::view("Interserver_connection was lost"));
	boost::uint16_t magic_number = boost::numeric_cast<boost::uint16_t>(message_id);
	launch_deflate_and_send(magic_number, msg);
}

// Non-member functions.
void wait_for_response(Poseidon::Cbpp::Message_base &msg, const boost::shared_ptr<const Promised_response> &promise){
	PROFILE_ME;

	Interserver_response resp;
	resp.set_err_code(Protocol::error_reserved_response_destroyed);
	Poseidon::yield(promise);
	resp.swap(promise->get());
	LOG_CIRCE_TRACE("Response: ", resp);

	CIRCE_PROTOCOL_THROW_UNLESS(resp.get_err_code() == 0, resp.get_err_code(), Poseidon::Rcnts(resp.get_err_msg()));
	DEBUG_THROW_UNLESS(resp.get_message_id() != 0, Poseidon::Exception, Poseidon::Rcnts::view("No message payload but status code returned"));
	DEBUG_THROW_UNLESS(resp.get_message_id() == msg.get_id(), Poseidon::Exception, Poseidon::Rcnts::view("Unexpected response message ID"));
	msg.deserialize(resp.get_payload());
	if(!resp.get_payload().empty()){
		LOG_CIRCE_WARNING("Ignoring ", resp.get_payload().size(), " redundant byte(s) after message ", msg);
	}
}

}
}
