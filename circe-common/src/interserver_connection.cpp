// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connection.hpp"
#include "cbpp_response.hpp"
#include "mmain.hpp"
#include <poseidon/socket_base.hpp>
#include <poseidon/sha256.hpp>
#include <poseidon/zlib.hpp>
#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/singletons/workhorse_camp.hpp>
#include <poseidon/job_base.hpp>
#include <poseidon/singletons/job_dispatcher.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace Circe {
namespace Common {

enum {
	// Prefixes
	MSGFL_IS_RESPONSE     = 0x8000,  // uint32       serial
	                                 // int64        err_code
	                                 // flexible     if message_id == 0
	                                 //                err_msg
	                                 //              else
	                                 //                payload
	MSGFL_WANTS_RESPONSE  = 0x4000,  // uint32       serial
	MSGFL_SYSTEM          = 0x2000,  //
	MSGFL_RESERVED        = 0x1000,  //

	// System Messages
	MSG_CLIENT_HELLO      = 0x2001,  // byte[16]     connection_uuid
	                                 // byte[12]     nonce
	                                 // byte[16]     checksum_high = SHA-256(connection_uuid + '?' + nonce + ':' + application_key).slice(0, 16)
	MSG_SERVER_HELLO      = 0x2002,  // byte[16]     checksum_high = SHA-256(connection_uuid + '!' + nonce + ':' + application_key).slice(0, 16)
};

namespace {
	STD_EXCEPTION_PTR make_connection_lost_exception()
	try {
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("InterserverConnection is lost before a response could be received"));
	} catch(...){
		return STD_CURRENT_EXCEPTION();
	}
	const AUTO(g_connection_lost_exception, make_connection_lost_exception());
}

class InterserverConnection::RandomByteGenerator {
private:
	boost::random::mt19937 m_prng;
	boost::uint64_t m_word;

public:
	RandomByteGenerator(const unsigned char *seed_data, std::size_t seed_size)
		: m_prng(seed_data, seed_data + seed_size), m_word(1)
	{ }

public:
	unsigned char get(){
		boost::uint64_t word = m_word;
		if(word == 1){
			word = (word << 32) | m_prng();
		}
		m_word = word >> 8;
		return word & 0xFF;
	}
	void seed(const unsigned char *seed_data, std::size_t seed_size){
		m_prng.seed(seed_data, seed_data + seed_size);
		m_word = 1;
	}
};

class InterserverConnection::MessageFilter {
private:
	// Decoder
	RandomByteGenerator m_decryptor_prng;
	Poseidon::Inflator m_inflator;
	// Encoder
	RandomByteGenerator m_encryptor_prng;
	Poseidon::Deflator m_deflator;

public:
	MessageFilter(const std::string &application_key, int compression_level)
		: m_decryptor_prng(reinterpret_cast<const unsigned char *>(application_key.c_str()), application_key.size() + 1), m_inflator(false)
		, m_encryptor_prng(reinterpret_cast<const unsigned char *>(application_key.c_str()), application_key.size() + 1), m_deflator(false, compression_level)
	{ }
	~MessageFilter(){
		// Silence the warnings.
		m_inflator.clear();
		m_deflator.clear();
	}

public:
	Poseidon::StreamBuffer decode(Poseidon::StreamBuffer encoded_payload){
		PROFILE_ME;

		std::string temp;
		// Step 1: Decrypt the payload.
		temp.reserve(encoded_payload.size() + 4);
		temp.resize(encoded_payload.size());
		DEBUG_THROW_ASSERT((encoded_payload.get(&*temp.begin(), temp.size()) == temp.size()) && encoded_payload.empty());
		for(AUTO(it, temp.begin()); it != temp.end(); ++it){
			*it ^= m_decryptor_prng.get();
		}
		// Step 2: Append the terminator bytes to this block.
		temp.append("\x00\x00\xFF\xFF", 4);
		// Step 3: Inflate the buffer.
		m_inflator.put(temp);
		m_inflator.flush();
		Poseidon::StreamBuffer magic_payload = STD_MOVE(m_inflator.get_buffer());
		m_inflator.get_buffer().clear();
		return magic_payload;
	}
	void reseed_decoder_prng(const void *seed_data, std::size_t seed_size){
		PROFILE_ME;

		m_decryptor_prng.seed(static_cast<const unsigned char *>(seed_data), seed_size);
	}
	Poseidon::StreamBuffer encode(Poseidon::StreamBuffer magic_payload){
		PROFILE_ME;

		std::string temp;
		// Step 1: Deflate the buffer.
		m_deflator.put(magic_payload);
		m_deflator.flush();
		temp = m_deflator.get_buffer().dump_string();
		m_deflator.get_buffer().clear();
		DEBUG_THROW_ASSERT((temp.size() >= 4) && (temp.compare(temp.size() - 4, 4, "\x00\x00\xFF\xFF", 4) == 0));
		// Step 2: Drop the the terminator bytes from this block.
		temp.erase(temp.size() - 4);
		// Step 3: Encrypt the payload.
		for(AUTO(it, temp.begin()); it != temp.end(); ++it){
			*it ^= m_encryptor_prng.get();
		}
		Poseidon::StreamBuffer encoded_payload = Poseidon::StreamBuffer(temp);
		return encoded_payload;
	}
	void reseed_encoder_prng(const void *seed_data, std::size_t seed_size){
		PROFILE_ME;

		m_encryptor_prng.seed(static_cast<const unsigned char *>(seed_data), seed_size);
	}
};

class InterserverConnection::RequestMessageJob : public Poseidon::JobBase {
private:
	const Poseidon::SocketBase::DelayedShutdownGuard m_guard;
	const boost::weak_ptr<InterserverConnection> m_weak_connection;

	boost::uint16_t m_message_id;
	bool m_send_response;
	boost::uint32_t m_serial;
	Poseidon::StreamBuffer m_payload;

public:
	RequestMessageJob(const boost::shared_ptr<InterserverConnection> &connection, boost::uint16_t message_id, bool send_response, boost::uint32_t serial, Poseidon::StreamBuffer payload)
		: m_guard(boost::dynamic_pointer_cast<Poseidon::SocketBase>(connection)), m_weak_connection(connection)
		, m_message_id(message_id), m_send_response(send_response), m_serial(serial), m_payload(STD_MOVE(payload))
	{ }

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
			connection->sync_dispatch_message(m_message_id, m_send_response, m_serial, STD_MOVE(m_payload));
			LOG_CIRCE_TRACE("Done dispatching message: message_id = ", m_message_id);
		} catch(Poseidon::Cbpp::Exception &e){
			LOG_CIRCE_ERROR("Poseidon::Cbpp::Exception thrown: ", e.get_code(), ": ", e.what());
			connection->shutdown(e.get_code(), e.what());
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
			connection->shutdown(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
		}
	}
};

void InterserverConnection::inflate_and_dispatch(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &encoded_payload){
	PROFILE_ME;

	const AUTO(connection, weak_connection.lock());
	if(!connection){
		return;
	}
	LOG_CIRCE_TRACE("Inflate and dispatch: connection = ", (void *)connection.get());
	try {
		const std::size_t size_deflated = encoded_payload.size();
		Poseidon::StreamBuffer magic_payload = connection->m_message_filter->decode(STD_MOVE(encoded_payload));
		const std::size_t size_inflated = magic_payload.size();
		LOG_CIRCE_TRACE("Inflate result: ", size_deflated, " / ", size_inflated, " (", std::fixed, std::setprecision(3), (size_inflated != 0) ? size_deflated * 100.0 / size_inflated : 0.0, "%)");

		switch(magic_number){
		case MSG_CLIENT_HELLO: {
			Poseidon::Uuid connection_uuid;
			DEBUG_THROW_UNLESS(magic_payload.get(connection_uuid.data(), 16) == 16, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
				Poseidon::sslit("Unexpected end of stream encountered while parsing MSG_CLIENT_HELLO, expecting connection_uuid"));
			boost::array<unsigned char, 12> nonce;
			DEBUG_THROW_UNLESS(magic_payload.get(nonce.data(), 12) == 12, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
				Poseidon::sslit("Unexpected end of stream encountered while parsing MSG_CLIENT_HELLO, expecting nonce"));
			boost::array<unsigned char, 16> checksum_high;
			DEBUG_THROW_UNLESS(magic_payload.get(checksum_high.data(), 16) == 16, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
				Poseidon::sslit("Unexpected end of stream encountered while parsing MSG_CLIENT_HELLO, expecting checksum_high"));
			DEBUG_THROW_UNLESS(magic_payload.empty(), Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_JUNK_AFTER_PACKET,
				Poseidon::sslit("Junk after MSG_CLIENT_HELLO"));
			const AUTO(checksum, connection->calculate_checksum(connection_uuid, nonce, false));
			DEBUG_THROW_UNLESS(std::memcmp(checksum.data(), checksum_high.data(), 16) == 0, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_AUTHORIZATION_FAILURE,
				Poseidon::sslit("Request checksum mismatch"));
			connection->server_accept_hello(connection_uuid, nonce);
			break; }

		case MSG_SERVER_HELLO : {
			DEBUG_THROW_ASSERT(connection->is_connection_uuid_set());
			boost::array<unsigned char, 16> checksum_high;
			DEBUG_THROW_UNLESS(magic_payload.get(checksum_high.data(), 16) == 16, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
				Poseidon::sslit("Unexpected end of stream encountered while parsing MSG_SERVER_HELLO, expecting checksum_high"));
			DEBUG_THROW_UNLESS(magic_payload.empty(), Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_JUNK_AFTER_PACKET,
				Poseidon::sslit("Junk after MSG_SERVER_HELLO"));
			const AUTO(checksum, connection->calculate_checksum(connection->m_connection_uuid, connection->m_nonce, true));
			DEBUG_THROW_UNLESS(std::memcmp(checksum.data(), checksum_high.data(), 16) == 0, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_AUTHORIZATION_FAILURE,
				Poseidon::sslit("Response checksum mismatch"));
			break; }

		default: {
			DEBUG_THROW_UNLESS(Poseidon::has_none_flags_of(magic_number, MSGFL_SYSTEM & MSGFL_RESERVED), Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_BAD_REQUEST,
				Poseidon::sslit("Reserved bits in magic_number set"));
			BOOST_STATIC_ASSERT((MESSAGE_ID_MAX & (MESSAGE_ID_MAX + 1)) == 0);
			boost::uint16_t message_id = magic_number & MESSAGE_ID_MAX;
			if(Poseidon::has_any_flags_of(magic_number, MSGFL_IS_RESPONSE)){
				boost::uint32_t temp32;
				DEBUG_THROW_UNLESS(magic_payload.get(&temp32, 4) == 4, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
					Poseidon::sslit("Unexpected end of stream encountered while parsing user message, expecting serial"));
				boost::uint32_t serial = Poseidon::load_be(temp32);
				boost::int64_t temp64s;
				DEBUG_THROW_UNLESS(magic_payload.get(&temp64s, 8) == 8, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
					Poseidon::sslit("Unexpected end of stream encountered while parsing user message, expecting err_code"));
				long err_code = Poseidon::load_be(temp64s);
				CbppResponse resp;
				if(message_id == 0){
					resp = CbppResponse(err_code, magic_payload.dump_string());
				} else {
					resp = CbppResponse(message_id, STD_MOVE_IDN(magic_payload));
				}
				// Satisfy the promise.
				boost::shared_ptr<PromisedResponse> promise;
				{
					const Poseidon::Mutex::UniqueLock lock(connection->m_mutex);
					const AUTO(it, connection->m_weak_promises.find(serial));
					if(it != connection->m_weak_promises.end()){
						promise = it->second.lock();
						connection->m_weak_promises.erase(it);
					}
				}
				if(promise){
					promise->set_success(STD_MOVE(resp));
				}
			} else {
				bool wants_response = false;
				boost::uint32_t serial = 0;
				if(Poseidon::has_any_flags_of(magic_number, MSGFL_WANTS_RESPONSE)){
					boost::uint32_t temp32;
					DEBUG_THROW_UNLESS(magic_payload.get(&temp32, 4) == 4, Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_END_OF_STREAM,
						Poseidon::sslit("Unexpected end of stream encountered while parsing user message, expecting serial"));
					serial = Poseidon::load_be(temp32);
					wants_response = true;
				}
				Poseidon::JobDispatcher::enqueue(
					boost::make_shared<RequestMessageJob>(connection, message_id, wants_response, serial, STD_MOVE(magic_payload)),
					VAL_INIT);
			}
			break; }
		}

		if(magic_number == MSG_CLIENT_HELLO){
			connection->m_message_filter->reseed_decoder_prng(connection->m_nonce.data(), connection->m_nonce.size());
		}
	} catch(Poseidon::Cbpp::Exception &e){
		LOG_CIRCE_ERROR("Poseidon::Cbpp::Exception thrown: ", e.get_code(), ": ", e.what());
		connection->shutdown(e.get_code(), e.what());
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
		connection->shutdown(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
	}
}
void InterserverConnection::deflate_and_send(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &magic_payload){
	PROFILE_ME;

	const AUTO(connection, weak_connection.lock());
	if(!connection){
		return;
	}
	LOG_CIRCE_TRACE("Deflate and send: connection = ", (void *)connection.get());
	try {
		const std::size_t size_inflated = magic_payload.size();
		Poseidon::StreamBuffer encoded_payload = connection->m_message_filter->encode(STD_MOVE(magic_payload));
		const std::size_t size_deflated = encoded_payload.size();
		LOG_CIRCE_TRACE("Deflate result: ", size_deflated, " / ", size_inflated, " (", std::fixed, std::setprecision(3), (size_inflated != 0) ? size_deflated * 100.0 / size_inflated : 0.0, "%)");

		connection->layer5_send_data(magic_number, STD_MOVE(encoded_payload));

		if(magic_number == MSG_CLIENT_HELLO){
			connection->m_message_filter->reseed_encoder_prng(connection->m_nonce.data(), connection->m_nonce.size());
		}
	} catch(Poseidon::Cbpp::Exception &e){
		LOG_CIRCE_ERROR("Poseidon::Cbpp::Exception thrown: ", e.get_code(), ": ", e.what());
		connection->shutdown(e.get_code(), e.what());
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
		connection->shutdown(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
	}
}

std::size_t InterserverConnection::get_max_message_size(){
	return get_config<std::size_t>("interserver_max_message_size", 1048576);
}
int InterserverConnection::get_compression_level(){
	return get_config<int>("interserver_compression_level", 6);
}
boost::uint64_t InterserverConnection::get_hello_timeout(){
	return get_config<boost::uint64_t>("interserver_hello_timeout", 60000);
}

InterserverConnection::InterserverConnection(const std::string &application_key)
	: m_application_key(application_key), m_message_filter(new MessageFilter(application_key, get_compression_level()))
	, m_connection_uuid(), m_next_serial()
{
	LOG_CIRCE_INFO("InterserverConnection constructor: this = ", (void *)this);
}
InterserverConnection::~InterserverConnection(){
	LOG_CIRCE_INFO("InterserverConnection destructor: this = ", (void *)this);
}

boost::array<unsigned char, 12> InterserverConnection::create_nonce() const {
	PROFILE_ME;

	boost::array<unsigned char, 12> nonce;
	for(unsigned i = 0; i < 12; ++i){
		nonce[i] = Poseidon::random_uint32();
	}
	return nonce;
}
boost::array<unsigned char, 32> InterserverConnection::calculate_checksum(const Poseidon::Uuid &connection_uuid, const boost::array<unsigned char, 12> &nonce, bool response) const {
	PROFILE_ME;

	Poseidon::Sha256_ostream sha_os;
	sha_os.write((const char *)connection_uuid.data(), 16)
	      .put(response ? '!' : '?')
	      .write((const char *)nonce.data(), 12)
	      .put(':')
	      .write(m_application_key.data(), (long)m_application_key.size());
	return sha_os.finalize();
}

bool InterserverConnection::is_connection_uuid_set() const NOEXCEPT {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	return !!m_connection_uuid;
}
void InterserverConnection::server_accept_hello(const Poseidon::Uuid &connection_uuid, const boost::array<unsigned char, 12> &nonce){
	PROFILE_ME;

	LOG_CIRCE_INFO("Accepted HELLO from ", layer5_get_remote_info());
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		DEBUG_THROW_UNLESS(!m_connection_uuid, Poseidon::Exception, Poseidon::sslit("server_accept_hello() shall be called exactly once by the server and must not be called by the client"));
		{
			// Send the hello message to the client.
			Poseidon::StreamBuffer magic_payload;
			// Put the higher half of the response checksum (16 bytes).
			const AUTO(checksum, calculate_checksum(connection_uuid, nonce, true));
			magic_payload.put(checksum.data(), 16);
			// Send it!
			launch_deflate_and_send(MSG_SERVER_HELLO, STD_MOVE(magic_payload));
		}
		m_connection_uuid = connection_uuid;
		m_nonce = nonce;
	}
	layer7_post_set_connection_uuid();
}

void InterserverConnection::launch_inflate_and_dispatch(boost::uint16_t magic_number, Poseidon::StreamBuffer encoded_payload){
	PROFILE_ME;

	Poseidon::WorkhorseCamp::enqueue(VAL_INIT,
		boost::bind(&InterserverConnection::inflate_and_dispatch, virtual_weak_from_this<InterserverConnection>(), magic_number, STD_MOVE_IDN(encoded_payload)),
		reinterpret_cast<std::size_t>(this));
}
void InterserverConnection::launch_deflate_and_send(boost::uint16_t magic_number, Poseidon::StreamBuffer magic_payload){
	PROFILE_ME;

	Poseidon::WorkhorseCamp::enqueue(VAL_INIT,
		boost::bind(&InterserverConnection::deflate_and_send, virtual_weak_from_this<InterserverConnection>(), magic_number, STD_MOVE_IDN(magic_payload)),
		reinterpret_cast<std::size_t>(this));
}

void InterserverConnection::sync_dispatch_message(boost::uint16_t message_id, bool send_response, boost::uint32_t serial, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	CbppResponse resp;
	try {
		resp = layer7_on_sync_message(message_id, STD_MOVE(payload));
		DEBUG_THROW_UNLESS(is_message_id_valid(resp.get_message_id()), Poseidon::Exception, Poseidon::sslit("message_id out of range"));
	} catch(Poseidon::Cbpp::Exception &e){
		LOG_CIRCE_ERROR("Poseidon::Cbpp::Exception thrown: mesasge_id = ", message_id, ", err_code = ", e.get_code(), ", err_msg = ", e.what());
		resp = CbppResponse(e.get_code(), e.what());
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: mesasge_id = ", message_id, ", err_msg = ", e.what());
		resp = CbppResponse(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
	}
	if(send_response){
		boost::uint16_t magic_number = resp.get_message_id();
		Poseidon::StreamBuffer magic_payload;
		// Append the serial number in big-endian order.
		boost::uint32_t temp32;
		Poseidon::store_be(temp32, serial);
		magic_payload.put(&temp32, 4);
		// Append the error code in big-endian order.
		boost::int64_t temp64s;
		Poseidon::store_be(temp64s, resp.get_err_code());
		magic_payload.put(&temp64s, 8);
		// If the message ID is zero, append the error message. Otherwise, append the message payload.
		if(resp.get_message_id() == 0){
			magic_payload.put(resp.get_err_msg());
		} else {
			magic_payload.splice(resp.get_payload());
		}
		Poseidon::add_flags(magic_number, MSGFL_IS_RESPONSE);
		launch_deflate_and_send(magic_number, STD_MOVE(magic_payload));
	}
}

void InterserverConnection::layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::StreamBuffer encoded_payload){
	PROFILE_ME;

	launch_inflate_and_dispatch(magic_number, STD_MOVE(encoded_payload));
}
void InterserverConnection::layer5_on_receive_control(long status_code, Poseidon::StreamBuffer param){
	PROFILE_ME;
	DEBUG_THROW_ASSERT(is_connection_uuid_set());

	switch(status_code){
	case Poseidon::Cbpp::ST_SHUTDOWN:
		LOG_CIRCE_INFO("Received SHUTDOWN frame from ", layer5_get_remote_info());
		layer5_send_control(Poseidon::Cbpp::ST_SHUTDOWN, STD_MOVE(param));
		layer5_shutdown();
		break;
	case Poseidon::Cbpp::ST_PING:
		LOG_CIRCE_DEBUG("Received PING frame from ", get_remote_info(), ": param = ", param);
		layer5_send_control(Poseidon::Cbpp::ST_PONG, STD_MOVE(param));
		break;
	case Poseidon::Cbpp::ST_PONG:
		LOG_CIRCE_DEBUG("Received PONG frame from ", get_remote_info(), ": param = ", param);
		break;
	default:
		LOG_CIRCE_WARNING("Received CBPP error from ", get_remote_info(), ": status_code = ", status_code, ", param = ", param);
		break;
	}
}
void InterserverConnection::layer4_on_close(){
	PROFILE_ME;

	VALUE_TYPE(m_weak_promises) weak_promises;
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		DEBUG_THROW_ASSERT(layer5_has_been_shutdown());
		weak_promises.swap(m_weak_promises);
	}
	for(AUTO(it, weak_promises.begin()); it != weak_promises.end(); ++it){
		const AUTO(promise, it->second.lock());
		if(!promise){
			continue;
		}
		promise->set_exception(g_connection_lost_exception, false);
	}
}

void InterserverConnection::layer7_client_say_hello(){
	PROFILE_ME;

	const AUTO(connection_uuid, Poseidon::Uuid::random());
	const AUTO(nonce, create_nonce());
	LOG_CIRCE_INFO("Sending HELLO to ", layer5_get_remote_info());
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		DEBUG_THROW_UNLESS(!m_connection_uuid, Poseidon::Exception, Poseidon::sslit("layer7_client_say_hello() shall be called exactly once by the client and must not be called by the server"));
		{
			Poseidon::StreamBuffer magic_payload;
			// Put the connection UUID (16 bytes).
			magic_payload.put(connection_uuid.data(), 16);
			// Put the nonce (12 bytes).
			magic_payload.put(nonce.data(), 12);
			// Put the higher half of the request checksum (16 bytes).
			const AUTO(checksum, calculate_checksum(connection_uuid, nonce, false));
			magic_payload.put(checksum.data(), 16);
			// Send it!
			launch_deflate_and_send(MSG_CLIENT_HELLO, STD_MOVE(magic_payload));
		}
		m_connection_uuid = connection_uuid;
		m_nonce = nonce;
	}
	layer7_post_set_connection_uuid();
}

const Poseidon::Uuid &InterserverConnection::get_connection_uuid() const {
	DEBUG_THROW_UNLESS(is_connection_uuid_set(), Poseidon::Exception, Poseidon::sslit("InterserverConnection UUID has not been set"));
	return m_connection_uuid;
}

void InterserverConnection::send(const boost::shared_ptr<PromisedResponse> &promise, boost::uint16_t message_id, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending to ", layer5_get_remote_info(), ", message_id = ", message_id, ", payload.size() = ", payload.size());
	DEBUG_THROW_UNLESS(is_message_id_valid(message_id), Poseidon::Exception, Poseidon::sslit("message_id out of range"));

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(!layer5_has_been_shutdown(), Poseidon::Exception, Poseidon::sslit("InterserverConnection is lost"));
	AUTO(it, m_weak_promises.end());
	try {
		boost::uint16_t magic_number = message_id;
		Poseidon::StreamBuffer magic_payload;
		if(promise){
			boost::uint32_t serial = ++m_next_serial;
			it = m_weak_promises.emplace(serial, promise);
			// Append the serial number in big-endian order.
			boost::uint32_t temp32;
			Poseidon::store_be(temp32, serial);
			magic_payload.put(&temp32, 4);
			Poseidon::add_flags(magic_number, MSGFL_WANTS_RESPONSE);
		}
		magic_payload.splice(payload);
		launch_deflate_and_send(magic_number, STD_MOVE(magic_payload));
	} catch(...){
		if(it != m_weak_promises.end()){
			m_weak_promises.erase(it);
		}
		throw;
	}
}
void InterserverConnection::send(const boost::shared_ptr<PromisedResponse> &promise, const Poseidon::Cbpp::MessageBase &msg){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending to ", layer5_get_remote_info(), ", msg = ", msg);
	const boost::uint64_t message_id = msg.get_id();
	DEBUG_THROW_UNLESS(is_message_id_valid(message_id), Poseidon::Exception, Poseidon::sslit("message_id out of range"));
	send(promise, message_id, msg);
}
bool InterserverConnection::shutdown(long err_code, const char *err_msg) NOEXCEPT
try {
	PROFILE_ME;

	layer5_send_control(err_code, Poseidon::StreamBuffer(err_msg));
	return layer5_shutdown();
} catch(std::exception &e){
	LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
	layer4_force_shutdown();
	return false;
}

}
}
