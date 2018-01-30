// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017 - 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "compass_lock.hpp"
#include "common/interserver_connection.hpp"

namespace Circe {
namespace Pilot {

CompassLock::CompassLock(){ }
CompassLock::~CompassLock(){ }

void CompassLock::collect_expired_connections(){
	PROFILE_ME;

	bool erase_it;
	for(AUTO(it, m_readers.begin()); it != m_readers.end(); erase_it ? (it = m_readers.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
	for(AUTO(it, m_writers.begin()); it != m_writers.end(); erase_it ? (it = m_writers.erase(it)) : ++it){
		erase_it = it->second.expired();
	}
}

bool CompassLock::is_locked_shared(){
	PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is at least a reader and it is not a writer at the same time.
	return !m_readers.empty() && m_writers.empty();
}
bool CompassLock::is_locked_shared_by(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is such a reader, regardless of other readers or writers if any.
	return m_readers.find(connection_uuid) != m_readers.end();
}
bool CompassLock::try_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection){
	PROFILE_ME;

	collect_expired_connections();
	// Allow recursive locking.
	if(m_readers.find(connection->get_connection_uuid()) == m_readers.end()){
		// At the first attempt, succeed unless there is a writer other than this connection itself.
		if(m_writers.lower_bound(connection->get_connection_uuid()) != m_writers.begin()){
			return false;
		}
		if(m_writers.upper_bound(connection->get_connection_uuid()) != m_writers.end()){
			return false;
		}
	}
	m_readers.emplace(connection->get_connection_uuid(), connection);
	return true;
}
void CompassLock::release_lock_shared(const boost::shared_ptr<Common::InterserverConnection> &connection){
	PROFILE_ME;

	collect_expired_connections();
	// Throw an exception if such a reader cannot be found.
	const AUTO(it, m_readers.find(connection->get_connection_uuid()));
	DEBUG_THROW_UNLESS(it != m_readers.end(), Poseidon::Exception, Poseidon::sslit("CompassLock is not held by this connection in shared mode"));
	m_readers.erase(it);
}

bool CompassLock::is_locked_exclusive(){
	PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is a writer.
	return !m_writers.empty();
}
bool CompassLock::is_locked_exclusive_by(const Poseidon::Uuid &connection_uuid){
	PROFILE_ME;

	collect_expired_connections();
	// Return `true` if and only if there is such a writer, regardless of other readers or writers if any.
	return m_writers.find(connection_uuid) != m_writers.end();
}
bool CompassLock::try_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection){
	PROFILE_ME;

	collect_expired_connections();
	// Allow recursive locking.
	if(m_writers.find(connection->get_connection_uuid()) == m_writers.end()){
		// At the first attempt, succeed unless there is a reader other than this connection itself or another writer.
		if(m_readers.lower_bound(connection->get_connection_uuid()) != m_readers.begin()){
			return false;
		}
		if(m_readers.upper_bound(connection->get_connection_uuid()) != m_readers.end()){
			return false;
		}
		if(!m_writers.empty()){
			return false;
		}
	}
	m_writers.emplace(connection->get_connection_uuid(), connection);
	return true;
}
void CompassLock::release_lock_exclusive(const boost::shared_ptr<Common::InterserverConnection> &connection){
	PROFILE_ME;

	collect_expired_connections();
	// Throw an exception if such a writer cannot be found.
	const AUTO(it, m_writers.find(connection->get_connection_uuid()));
	DEBUG_THROW_UNLESS(it != m_writers.end(), Poseidon::Exception, Poseidon::sslit("CompassLock is not held by this connection in exclusive mode"));
	m_writers.erase(it);
}

}
}
