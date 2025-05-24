#include "logging.hpp"
#include "str.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <mutex>
namespace llarp::log
{

  Level g_currentLevel = Level::lvl_info;

  std::mutex g_loggers_access;
  std::unordered_map<std::string, CategoryLogger_ptr> g_loggers;

  CategoryLogger_ptr
  Cat(std::string_view _name)
  {
    std::string name{_name};
    auto lock = std::unique_lock(g_loggers_access);
    auto itr = g_loggers.find(name);
    if (itr != g_loggers.end())
      return itr->second;

    CategoryLogger_ptr logger = g_loggers[name] = std::make_shared<CategoryLogger>(_name);
    return logger;
  }

  Level
  level_from_string(std::string_view str)
  {
    if (llarp::IsFalseValue(str))
      return Level::off;
    if (llarp::string_iequal(str, "trace"))
      return Level::lvl_trace;
    if (llarp::string_iequal(str, "debug"))
      return Level::lvl_debug;
    if (llarp::string_iequal(str, "info"))
      return Level::lvl_info;
    if (llarp::string_iequal(str, "warn") or llarp::string_iequal(str, "warning"))
      return Level::lvl_warning;
    if (llarp::string_iequal(str, "error"))
      return Level::lvl_error;
    throw std::invalid_argument{fmt::format("invalid log level: {}", str)};
  }

  void
  set_log_level(Level lvl)
  {
    auto lock = std::unique_lock(g_loggers_access);
    for (const auto& [key, val] : g_loggers)
    {
      val->min_level(lvl);
    }
  }

  CategoryLogger::CategoryLogger(std::string_view name) : m_MinLevel{g_currentLevel}, m_Name{name}
  {}

  bool
  CategoryLogger::should_log(Level lvl) const
  {
    return m_MinLevel <= lvl;
  }

  void
  CategoryLogger::min_level(Level lvl)
  {
    m_MinLevel = lvl;
  }

  void
  CategoryLogger::on_log_event(
      Level lvl, const llarp::util::source_location& loc, const std::string& msg) const
  {
    switch (lvl)
    {
      case Level::lvl_trace:
        spdlog::trace("[{}] [{}|{}:{}] {}", uptime(), m_Name, loc.file_name(), loc.line(), msg);
        return;
      case Level::lvl_debug:
        spdlog::debug("[{}] [{}|{}:{}] {}", uptime(), m_Name, loc.file_name(), loc.line(), msg);
        return;
      case Level::lvl_info:
        spdlog::info("[{}] [{}|{}:{}] {}", uptime(), m_Name, loc.file_name(), loc.line(), msg);
        return;
      case Level::lvl_warning:
        spdlog::warn("[{}] [{}|{}:{}] {}", uptime(), m_Name, loc.file_name(), loc.line(), msg);
        return;
      case Level::lvl_error:
        spdlog::error("[{}] [{}|{}:{}] {}", uptime(), m_Name, loc.file_name(), loc.line(), msg);
        return;
    }
  }

}  // namespace llarp::log
