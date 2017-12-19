// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_PRECOMPILED_HPP_
#define CIRCE_COMMON_PRECOMPILED_HPP_

#include <poseidon/precompiled.hpp>
#include <poseidon/fwd.hpp>

#include <poseidon/shared_nts.hpp>
#include <poseidon/exception.hpp>
#include <poseidon/log.hpp>
#include <poseidon/profiler.hpp>
#include <poseidon/errno.hpp>
#include <poseidon/time.hpp>
#include <poseidon/random.hpp>
#include <poseidon/flags.hpp>
#include <poseidon/module_raii.hpp>
#include <poseidon/uuid.hpp>
#include <poseidon/endian.hpp>
#include <poseidon/string.hpp>
#include <poseidon/checked_arithmetic.hpp>
#include <poseidon/buffer_streams.hpp>
#include <poseidon/async_job.hpp>
#include <poseidon/atomic.hpp>
#include <poseidon/mutex.hpp>
#include <poseidon/recursive_mutex.hpp>

#ifdef POSEIDON_CXX11
#  include <cstdint>
#  include <array>
#  include <type_traits>
#endif

#define LOG_CIRCE(level_, ...)      LOG_MASK(0x4000 | (level_), __VA_ARGS__)
#define LOG_CIRCE_FATAL(...)        LOG_CIRCE(::Poseidon::Logger::LV_FATAL,   __VA_ARGS__)
#define LOG_CIRCE_ERROR(...)        LOG_CIRCE(::Poseidon::Logger::LV_ERROR,   __VA_ARGS__)
#define LOG_CIRCE_WARNING(...)      LOG_CIRCE(::Poseidon::Logger::LV_WARNING, __VA_ARGS__)
#define LOG_CIRCE_INFO(...)         LOG_CIRCE(::Poseidon::Logger::LV_INFO,    __VA_ARGS__)
#define LOG_CIRCE_DEBUG(...)        LOG_CIRCE(::Poseidon::Logger::LV_DEBUG,   __VA_ARGS__)
#define LOG_CIRCE_TRACE(...)        LOG_CIRCE(::Poseidon::Logger::LV_TRACE,   __VA_ARGS__)

#endif