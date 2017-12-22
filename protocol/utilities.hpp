// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_PROTOCOL_UTILITIES_HPP_
#define CIRCE_PROTOCOL_UTILITIES_HPP_

#include <poseidon/cxx_ver.hpp>
#include <poseidon/optional_map.hpp>
#include <boost/container/vector.hpp>

namespace Circe {
namespace Protocol {

template<typename DestinationT>
void copy_key_values(boost::container::vector<DestinationT> &dst, const Poseidon::OptionalMap &src){
	dst.reserve(dst.size() + src.size());
	for(AUTO(it, src.begin()); it != src.end(); ++it){
		dst.emplace_back();
		dst.back().key = it->first.get();
		dst.back().value = it->second;
	}
}
template<typename SourceT>
void copy_key_values(Poseidon::OptionalMap &dst, const boost::container::vector<SourceT> &src){
	for(AUTO(it, src.begin()); it != src.end(); ++it){
		dst.append(Poseidon::SharedNts(it->key), it->value);
	}
}
template<typename DestinationT, typename SourceT>
void copy_key_values(boost::container::vector<DestinationT> &dst, const boost::container::vector<SourceT> &src){
	dst.reserve(dst.size() + src.size());
	for(AUTO(it, src.begin()); it != src.end(); ++it){
		dst.emplace_back();
		dst.back().key = it->key;
		dst.back().value = it->value;
	}
}
template<typename SourceT>
Poseidon::OptionalMap copy_key_values(const boost::container::vector<SourceT> &src){
	Poseidon::OptionalMap dst;
	((copy_key_values))(dst, src);
	return dst;
}

#ifdef POSEIDON_CXX11
template<typename DestinationT>
void copy_key_values(boost::container::vector<DestinationT> &dst, Poseidon::OptionalMap &&src){
	dst.reserve(dst.size() + src.size());
	for(auto it = src.begin(); it != src.end(); ++it){
		dst.emplace_back();
		dst.back().key = it->first.get();
		dst.back().value = std::move(it->second);
	}
}
template<typename SourceT>
void copy_key_values(Poseidon::OptionalMap &dst, boost::container::vector<SourceT> &&src){
	for(auto it = src.begin(); it != src.end(); ++it){
		dst.append(Poseidon::SharedNts(it->key), std::move(it->value));
	}
}
template<typename DestinationT, typename SourceT>
void copy_key_values(boost::container::vector<DestinationT> &dst, boost::container::vector<SourceT> &&src){
	dst.reserve(dst.size() + src.size());
	for(auto it = src.begin(); it != src.end(); ++it){
		dst.emplace_back();
		dst.back().key = std::move(it->key);
		dst.back().value = std::move(it->value);
	}
}
template<typename SourceT>
Poseidon::OptionalMap copy_key_values(boost::container::vector<SourceT> &&src){
	Poseidon::OptionalMap dst;
	((copy_key_values))(dst, std::move(src));
	return dst;
}
#endif

}
}

#endif
