// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_UTILITIES_HPP_
#define CIRCE_COMMON_UTILITIES_HPP_

#include <poseidon/cxx_ver.hpp>
#include <poseidon/optional_map.hpp>
#include <boost/container/vector.hpp>

namespace Circe {
namespace Common {

template<typename ElementT>
void copy_key_values(boost::container::vector<ElementT> &vec, const Poseidon::OptionalMap &map){
	vec.reserve(vec.size() + map.size());
	for(AUTO(it, map.begin()); it != map.end(); ++it){
		vec.emplace_back();
		vec.back().key = it->first.get();
		vec.back().value = it->second;
	}
}
template<typename ElementT>
void copy_key_values(Poseidon::OptionalMap &map, const boost::container::vector<ElementT> &vec){
	for(AUTO(it, vec.begin()); it != vec.end(); ++it){
		map.append(Poseidon::SharedNts(it->key), it->value);
	}
}

template<typename ElementT>
Poseidon::OptionalMap extract_key_values(const boost::container::vector<ElementT> &vec){
	Poseidon::OptionalMap map;
	((copy_key_values))(map, vec);
	return map;
}

}
}

#endif
