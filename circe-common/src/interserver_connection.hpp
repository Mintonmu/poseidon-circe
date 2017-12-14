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

namespace Circe {
namespace Common {

class CbppResponse;

class InterserverConnection : public virtual Poseidon::VirtualSharedFromThis {
public:
	enum {
		MESSAGE_ID_MIN  = 0x0040,
		MESSAGE_ID_MAX  = 0x0FFF,
	};

	class SyncMessageJob;

	typedef Poseidon::PromiseContainer<CbppResponse> PromisedResponse;

private:
	static void inflate_and_dispatch(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &payload);
	static void deflate_and_send(const boost::weak_ptr<InterserverConnection> &weak_connection, boost::uint16_t magic_number, Poseidon::StreamBuffer &payload);

private:
	// These are protected by a mutex and can be accessed by any thread.
	mutable Poseidon::Mutex m_mutex;
	Poseidon::Uuid m_uuid;
	boost::uint32_t m_serial;
	boost::container::flat_multimap<boost::uint32_t, boost::weak_ptr<PromisedResponse> > m_weak_promises;

	// These are only used by the workhorse thread.
	boost::scoped_ptr<Poseidon::Inflator> m_inflator;
	boost::scoped_ptr<Poseidon::Deflator> m_deflator;

public:
	InterserverConnection();
	~InterserverConnection() OVERRIDE;

private:
	void sync_do_dispatch(boost::uint16_t magic_number, Poseidon::StreamBuffer payload);

protected:
	virtual const Poseidon::IpPort &layer5_get_remote_info() const NOEXCEPT = 0;
	virtual const Poseidon::IpPort &layer5_get_local_info() const NOEXCEPT = 0;
	virtual bool layer5_has_been_shutdown() const NOEXCEPT = 0;
	virtual bool layer5_shutdown() NOEXCEPT = 0;
	virtual void layer4_force_shutdown() NOEXCEPT = 0;
	virtual void layer5_send_data(boost::uint16_t magic_number, Poseidon::StreamBuffer payload) = 0;
	virtual void layer5_send_control(long status_code, Poseidon::StreamBuffer param) = 0;

	void layer5_on_receive_data(boost::uint16_t magic_number, Poseidon::StreamBuffer payload);
	void layer5_on_receive_control(long status_code, Poseidon::StreamBuffer param);
	void layer5_on_close();

	virtual CbppResponse layer7_on_sync_message(boost::uint16_t message_id, Poseidon::StreamBuffer payload) = 0;

public:
	// This function throws an exception if the UUID has not been set.
	const Poseidon::Uuid &get_uuid() const;

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
	bool shutdown(long err_code, const char *err_msg) NOEXCEPT;
};

}
}

#endif
