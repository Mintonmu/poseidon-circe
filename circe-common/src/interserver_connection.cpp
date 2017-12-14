// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "interserver_connection.hpp"
#include "mmain.hpp"
#include "cbpp_response.hpp"
#include <poseidon/socket_base.hpp>
#include <poseidon/zlib.hpp>
#include <poseidon/sha256.hpp>
#include <poseidon/cbpp/message_base.hpp>
#include <poseidon/singletons/workhorse_camp.hpp>
#include <poseidon/job_base.hpp>
#include <poseidon/singletons/job_dispatcher.hpp>

namespace Circe {
namespace Common {

// Message IDs.
enum {
	MSG_CLIENT_HELLO  = 0x0001,  // byte[16]     connection_uuid
	                             // byte[16]     cnonce
	                             // byte[32]     SHA-256(connection_uuid + '/' + cnonce + '/' + application_key)
	MSG_SERVER_HELLO  = 0x0002,  // byte[16]     snonce
	                             // byte[32]     SHA-256(connection_uuid + '/' + snonce + '/' + application_key)
	MSG_RESPONSE      = 0x0003,  // uint32le     serial_number
	                             // byte[]       payload
	// *                         // uint32le     serial_number
	                             // byte[]       payload
	MSGFL_WANTS_RESPONSE  = 0x8000,  // A serial number represented as UINT32LE is prepended to the payload.
	MSGFL_RESERVED_ONE    = 0x4000,  //
	MSGFL_RESERVED_TWO    = 0x2000,  //
	MSGFL_RESERVED_THREE  = 0x1000,  //
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

class InterserverConnection::SyncMessageJob : public Poseidon::JobBase {
private:
	const Poseidon::SocketBase::DelayedShutdownGuard m_guard;
	const boost::weak_ptr<InterserverConnection> m_weak_connection;

	boost::uint16_t m_magic_number;
	Poseidon::StreamBuffer m_payload;

public:
	SyncMessageJob(const boost::shared_ptr<InterserverConnection> &connection, boost::uint16_t magic_number, Poseidon::StreamBuffer payload)
		: m_guard(boost::dynamic_pointer_cast<Poseidon::SocketBase>(connection)), m_weak_connection(connection)
		, m_magic_number(magic_number), m_payload(STD_MOVE(payload))
	{ }

private:
	void do_dispatch(const boost::shared_ptr<InterserverConnection> &connection, boost::uint16_t magic_number, Poseidon::StreamBuffer payload);

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
			connection->sync_do_dispatch(m_magic_number, STD_MOVE(m_payload));
		} catch(Poseidon::Cbpp::Exception &e){
			LOG_CIRCE_WARNING("Poseidon::Cbpp::Exception thrown: ", e.get_code(), ": ", e.what());
			connection->shutdown(e.get_code(), e.what());
		} catch(std::exception &e){
			LOG_CIRCE_WARNING("std::exception thrown: ", e.what());
			connection->shutdown(Poseidon::Cbpp::ST_INTERNAL_ERROR, e.what());
		}
	}
};

void InterserverConnection::inflate_and_dispatch(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &payload){
	PROFILE_ME;

	const AUTO(connection, weak_connection.lock());
	if(!connection){
		return;
	}
	LOG_CIRCE_TRACE("Inflate and dispatch: connection = ", connection);

	try {
		const AUTO(size_compressed, payload.size());
		{
			if(!connection->m_inflator){
				connection->m_inflator.reset(new Poseidon::Inflator(false));
			}
			connection->m_inflator->put('\x00');
			connection->m_inflator->put('\x00');
			connection->m_inflator->put('\xFF');
			connection->m_inflator->put('\xFF');
			connection->m_inflator->put(payload);
			connection->m_inflator->flush();
			payload.clear();
			payload.swap(connection->m_inflator->get_buffer());
		}
		const AUTO(size_plain, payload.size());
		if(size_plain != 0){
			LOG_CIRCE_TRACE("Inflate result: ", size_compressed, " / ", size_plain, " (", std::fixed, std::setprecision(3), size_compressed * 100.0 / size_plain, "%)");
		}
		Poseidon::JobDispatcher::enqueue(
			boost::make_shared<SyncMessageJob>(connection, magic_number, STD_MOVE(payload)),
			VAL_INIT);
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
		connection->layer4_force_shutdown();
	}
}
void InterserverConnection::deflate_and_send(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &payload){
	PROFILE_ME;

	const AUTO(connection, weak_connection.lock());
	if(!connection){
		return;
	}
	LOG_CIRCE_TRACE("Deflate and send: connection = ", connection);

	try {
		const AUTO(size_plain, payload.size());
		{
			if(!connection->m_deflator){
				const AUTO(level, get_config<int>("interserver_compression_level", 6));
				connection->m_deflator.reset(new Poseidon::Deflator(false, level));
			}
			connection->m_inflator->put(payload);
			connection->m_inflator->flush();
			payload.clear();
			payload.swap(connection->m_inflator->get_buffer());
			DEBUG_THROW_ASSERT(payload.unput() == 0xFF);
			DEBUG_THROW_ASSERT(payload.unput() == 0xFF);
			DEBUG_THROW_ASSERT(payload.unput() == 0x00);
			DEBUG_THROW_ASSERT(payload.unput() == 0x00);
		}
		const AUTO(size_compressed, payload.size());
		if(size_plain != 0){
			LOG_CIRCE_TRACE("Deflate result: ", size_compressed, " / ", size_plain, " (", std::fixed, std::setprecision(3), size_compressed * 100.0 / size_plain, "%)");
		}
		connection->layer5_send_data(magic_number, STD_MOVE(payload));
	} catch(std::exception &e){
		LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
		connection->layer4_force_shutdown();
	}
}

InterserverConnection::InterserverConnection()
	: m_uuid(), m_serial()
{
	LOG_CIRCE_INFO("InterserverConnection constructor: this = ", (void *)this);
}
InterserverConnection::~InterserverConnection(){
	LOG_CIRCE_INFO("InterserverConnection destructor: this = ", (void *)this);
}


void InterserverConnection::sync_do_dispatch(boost::uint16_t magic_number, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	boost::shared_ptr<PromisedResponse> promise;
	boost::uint32_t serial;
	if(Poseidon::has_any_flags_of(magic_number, MSGFL_WANTS_RESPONSE)){
		boost::uint32_t serial_le;
		DEBUG_THROW_UNLESS(payload.get(&serial_le, 4) == 4, Poseidon::Exception, Poseidon::sslit("Unexpected end of stream encountered while parsing serial number"));
		serial = Poseidon::load_le(serial_le);
		// Here is a lock.
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		const AUTO(it, m_weak_promises.find(serial));
		if(it != m_weak_promises.end()){
			promise = it->second.lock();
			m_weak_promises.erase(it);
		}
	}
	if(Poseidon::has_any_flags_of(magic_number, MSGFL_RESERVED_ONE)){
		DEBUG_THROW(Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_FORBIDDEN, Poseidon::sslit("MSGFL_RESERVED_ONE set in magic_number"));
	}
	if(Poseidon::has_any_flags_of(magic_number, MSGFL_RESERVED_TWO)){
		DEBUG_THROW(Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_FORBIDDEN, Poseidon::sslit("MSGFL_RESERVED_TWO set in magic_number"));
	}
	if(Poseidon::has_any_flags_of(magic_number, MSGFL_RESERVED_THREE)){
		DEBUG_THROW(Poseidon::Cbpp::Exception, Poseidon::Cbpp::ST_FORBIDDEN, Poseidon::sslit("MSGFL_RESERVED_THREE set in magic_number"));
	}
	boost::uint16_t message_id = magic_number & MESSAGE_ID_MAX;
	AUTO(response, layer7_on_sync_message(message_id, STD_MOVE(payload)));
	if(promise){
		promise->set_success(STD_MOVE(response));
	}
}

void InterserverConnection::layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	Poseidon::WorkhorseCamp::enqueue(VAL_INIT,
		boost::bind(&InterserverConnection::inflate_and_dispatch, virtual_weak_from_this<InterserverConnection>(), magic_number, STD_MOVE(payload)),
		reinterpret_cast<std::size_t>(this));
}
void InterserverConnection::layer5_on_receive_control(long status_code, Poseidon::StreamBuffer param){
	PROFILE_ME;

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
void InterserverConnection::layer5_on_close(){
	PROFILE_ME;
	DEBUG_THROW_ASSERT(layer5_has_been_shutdown());

	VALUE_TYPE(m_weak_promises) weak_promises;
	{
		const Poseidon::Mutex::UniqueLock lock(m_mutex);
		weak_promises.swap(m_weak_promises);
	}
	for(AUTO(it, weak_promises.begin()); it != weak_promises.end(); ++it){
		const AUTO(promise, it->second.lock());
		if(!promise){
			continue;
		}
		try {
			promise->set_exception(g_connection_lost_exception);
		} catch(std::exception &e){
			LOG_CIRCE_ERROR("std::exception thrown: ", e.what());
		}
	}
}

const Poseidon::Uuid &InterserverConnection::get_uuid() const {
	PROFILE_ME;

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	DEBUG_THROW_UNLESS(m_uuid, Poseidon::Exception, Poseidon::sslit("InterserverConnection UUID has not been set"));
	return m_uuid;
}

void InterserverConnection::send(const boost::shared_ptr<PromisedResponse> &promise, boost::uint16_t message_id, Poseidon::StreamBuffer payload){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending to ", layer5_get_remote_info(), ", message_id = ", message_id, ", payload.size() = ", payload.size());
	DEBUG_THROW_UNLESS((MESSAGE_ID_MIN <= message_id) && (message_id <= MESSAGE_ID_MAX), Poseidon::Exception, Poseidon::sslit("message_id out of range"));

	const Poseidon::Mutex::UniqueLock lock(m_mutex);
	if(layer5_has_been_shutdown()){
		DEBUG_THROW(Poseidon::Exception, Poseidon::sslit("InterserverConnection is lost"));
	}
	boost::uint32_t serial;
	boost::uint16_t magic_number = message_id;
	if(promise){
		serial = ++m_serial;
		Poseidon::StreamBuffer temp;
		boost::uint32_t serial_le;
		Poseidon::store_le(serial_le, serial);
		temp.put(&serial_le, 4);
		temp.splice(payload);
		payload.swap(temp);
		Poseidon::add_flags(magic_number, MSGFL_WANTS_RESPONSE);
	}
	Poseidon::WorkhorseCamp::enqueue(VAL_INIT,
		boost::bind(&InterserverConnection::deflate_and_send, virtual_weak_from_this<InterserverConnection>(), magic_number, STD_MOVE(payload)),
		reinterpret_cast<std::size_t>(this));
	if(promise){
		m_weak_promises.emplace(serial, promise);
	}
}
void InterserverConnection::send(const boost::shared_ptr<PromisedResponse> &promise, const Poseidon::Cbpp::MessageBase &msg){
	PROFILE_ME;

	LOG_CIRCE_DEBUG("Sending to ", layer5_get_remote_info(), ", msg = ", msg);
	const boost::uint64_t message_id = msg.get_id();
	DEBUG_THROW_UNLESS((MESSAGE_ID_MIN <= message_id) && (message_id <= MESSAGE_ID_MAX), Poseidon::Exception, Poseidon::sslit("message_id out of range"));
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
