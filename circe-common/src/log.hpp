// 这个文件是 Circe 服务器应用程序框架的一部分。
// Copyleft 2017, LH_Mouse. All wrongs reserved.

#ifndef CIRCE_COMMON_LOG_HPP_
#define CIRCE_COMMON_LOG_HPP_

#include <poseidon/log.hpp>

#define LOG_CIRCE(level_, ...)      LOG_MASK(0x4000 | (level_), __VA_ARGS__)
#define LOG_CIRCE_FATAL(...)        LOG_CIRCE(::Poseidon::Logger::LV_FATAL,   __VA_ARGS__)
#define LOG_CIRCE_ERROR(...)        LOG_CIRCE(::Poseidon::Logger::LV_ERROR,   __VA_ARGS__)
#define LOG_CIRCE_WARNING(...)      LOG_CIRCE(::Poseidon::Logger::LV_WARNING, __VA_ARGS__)
#define LOG_CIRCE_INFO(...)         LOG_CIRCE(::Poseidon::Logger::LV_INFO,    __VA_ARGS__)
#define LOG_CIRCE_DEBUG(...)        LOG_CIRCE(::Poseidon::Logger::LV_DEBUG,   __VA_ARGS__)
#define LOG_CIRCE_TRACE(...)        LOG_CIRCE(::Poseidon::Logger::LV_TRACE,   __VA_ARGS__)

#endif