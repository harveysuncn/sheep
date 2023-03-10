#pragma once
#include <cstddef>
#define FMT_HEADER_ONLY
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <functional>

#include "log/logger.hpp"
#include "log/loglevel.hpp"

namespace sheep {

namespace log
{
  using HashFunc = std::hash<std::thread::id>;
  static HashFunc Hasher = HashFunc();
} // namespace log

}

#ifdef _MSC_VER
#define LOG_FUNC() __FUNCTION__
#elif defined(__clang__)
#define LOG_FUNC() __FUNCTION__
#elif defined(__BORLANDC__)
#define LOG_FUNC() __FUNC__
#else
#define LOG_FUNC() __PRETTY_FUNCTION__
#endif

#define __FILENAME__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_FILE() __FILENAME__
#define LOG_LINE() __LINE__
#define GET_TID() \
  log::Hasher(std::this_thread::get_id()) // std::thread::id -> std::size_t

#define trace(format, ...)                                                 \
  log(log::TRACE,                                            \
      FMT_STRING("{:%Y-%m-%d %H:%M:}{:%S} {} {} [{}:{}@{}] " format "\n"), \
      logLevelString(log::TRACE), GET_TID(), LOG_FUNC(), LOG_FILE(),    \
      LOG_LINE(), ##__VA_ARGS__)

#define debug(format, ...)                                                 \
  log(log::DEBUG,                                            \
      FMT_STRING("{:%Y-%m-%d %H:%M:}{:%S} {} {} [{}:{}@{}] " format "\n"), \
      logLevelString(log::DEBUG), GET_TID(), LOG_FUNC(), LOG_FILE(),    \
      LOG_LINE(), ##__VA_ARGS__)

#define info(format, ...)                                                  \
  log(log::INFO,                                             \
      FMT_STRING("{:%Y-%m-%d %H:%M:}{:%S} {} {} [{}:{}@{}] " format "\n"), \
      logLevelString(log::INFO), GET_TID(), LOG_FUNC(), LOG_FILE(),     \
      LOG_LINE(), ##__VA_ARGS__)

#define warn(format, ...)                                                  \
  log(log::WARN,                                             \
      FMT_STRING("{:%Y-%m-%d %H:%M:}{:%S} {} {} [{}:{}@{}] " format "\n"), \
      logLevelString(log::WARN), GET_TID(), LOG_FUNC(), LOG_FILE(),     \
      LOG_LINE(), ##__VA_ARGS__)

#define error(format, ...)                                                 \
  log(log::ERROR,                                            \
      FMT_STRING("{:%Y-%m-%d %H:%M:}{:%S} {} {} [{}:{}@{}] " format "\n"), \
      logLevelString(log::ERROR), GET_TID(), LOG_FUNC(), LOG_FILE(),    \
      LOG_LINE(), ##__VA_ARGS__)

#define sleepus(microSec) \
  std::this_thread::sleep_for(std::chrono::microseconds(microSec))

#define sleepms(milliSec) \
  std::this_thread::sleep_for(std::chrono::milliseconds(milliSec))

#define sleeps(second) \
  std::this_thread::sleep_for(std::chrono::seconds(second));
