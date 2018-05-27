// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_lock.hpp"
#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

Compass_lock::Compass_lock(){
	//
}
Compass_lock::~Compass_lock(){
	//
}

void Compass_lock::collect_expired_connections() const NOEXCEPT {
	POSEIDON_PROFILE_ME;

	bool erase_it;
	for(AUTO(it, m_readers.begin()); it != m_readers.end(); erase_it ? (it = m_readers.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
	for(AUTO(it, m_writers.begin()); it != m_writers.end(); erase_it ? (it = m_writers.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
}

bool Compass_lock::is_locked_shared() const NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is at least a reader and it is not a writer at the same time.
	return !m_readers.empty() && m_writers.empty();
}
bool Compass_lock::is_locked_shared_by(const Poseidon::Uuid &connection_uuid) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is such a reader, regardless of other readers or writers if any.
	return m_readers.find(connection_uuid) != m_readers.end();
}
bool Compass_lock::try_lock_shared(const boost::shared_ptr<Common::Interserver_connection> &connection){
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Allow recursive locking.
	if(m_readers.find(connection->get_connection_uuid()) == m_readers.end()){
		// At the first attempt, succeed unless there is a writer other than this connection itself.
		if(m_writers.equal_range(connection->get_connection_uuid()) != std::make_pair(m_writers.begin(), m_writers.end())){
			return false;
		}
	}
	m_readers.emplace(connection->get_connection_uuid(), connection);
	return true;
}
bool Compass_lock::release_lock_shared(const boost::shared_ptr<Common::Interserver_connection> &connection) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Throw an exception if such a reader cannot be found.
	const AUTO(it, m_readers.find(connection->get_connection_uuid()));
	if(it == m_readers.end()){
		return false;
	}
	m_readers.erase(it);
	return true;
}

bool Compass_lock::is_locked_exclusive() const NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is a writer.
	return !m_writers.empty();
}
bool Compass_lock::is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is such a writer, regardless of other readers or writers if any.
	return m_writers.find(connection_uuid) != m_writers.end();
}
bool Compass_lock::try_lock_exclusive(const boost::shared_ptr<Common::Interserver_connection> &connection){
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Allow recursive locking.
	if(m_writers.find(connection->get_connection_uuid()) == m_writers.end()){
		// At the first attempt, succeed unless there is a reader other than this connection itself or another writer.
		if(m_readers.equal_range(connection->get_connection_uuid()) != std::make_pair(m_readers.begin(), m_readers.end())){
			return false;
		}
		if(!m_writers.empty()){
			return false;
		}
	}
	m_writers.emplace(connection->get_connection_uuid(), connection);
	return true;
}
bool Compass_lock::release_lock_exclusive(const boost::shared_ptr<Common::Interserver_connection> &connection) NOEXCEPT {
	POSEIDON_PROFILE_ME;

	collect_expired_connections();
	// Throw an exception if such a writer cannot be found.
	const AUTO(it, m_writers.find(connection->get_connection_uuid()));
	if(it == m_writers.end()){
		return false;
	}
	m_writers.erase(it);
	return true;
}

}
}
