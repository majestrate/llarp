#include "logging.hpp"
#include "str.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string_view>
#include <mutex>
#include "time.hpp"
namespace llarp::log
{

  Level g_currentLevel = Level::lvl_info;

  static std::mutex g_loggers_access;
  static std::unique_ptr<std::unordered_map<std::string, CategoryLogger_ptr>> g_loggers = nullptr;

  CategoryLogger_ptr
  Cat(std::string_view _name)
  {
    std::string name{_name};
    auto lock = std::unique_lock(g_loggers_access);
    if (g_loggers == nullptr)
      g_loggers = std::make_unique<std::unordered_map<std::string, CategoryLogger_ptr>>();

    auto itr = g_loggers->find(name);
    if (itr != g_loggers->end())
      return itr->second;

    CategoryLogger_ptr logger = std::make_shared<CategoryLogger>(_name);
    g_loggers->emplace(std::make_pair(name, logger));
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

  auto
  to_spdlog_level(Level lvl)
  {
    switch (lvl)
    {
      case Level::lvl_trace:
        return spdlog::level::trace;
      case Level::lvl_debug:
        return spdlog::level::debug;
      case Level::lvl_info:
        return spdlog::level::info;
      case Level::lvl_warning:
        return spdlog::level::warn;
      case Level::lvl_error:
        return spdlog::level::err;
      default:
        return spdlog::level::critical;
    }
  }

  void
  set_log_level(Level lvl)
  {
    auto lock = std::unique_lock(g_loggers_access);
    for (const auto& [key, val] : *g_loggers)
    {
      val->min_level(lvl);
    }
    spdlog::set_level(to_spdlog_level(lvl));
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

  namespace
  {
    std::string_view
    format_sl(const llarp::util::source_location& loc)
    {
      static constexpr std::string_view source_prefix = LOGGING_SOURCE_ROOT;
      std::string_view filename{loc.file_name()};
      if (filename.substr(0, source_prefix.size()) == source_prefix)
      {
        filename.remove_prefix(source_prefix.size());
        if (!filename.empty() && filename[0] == '/')
          filename.remove_prefix(1);
      }

      while (filename.substr(0, 3) == "../")
        filename.remove_prefix(3);

      return filename;
    }
  }  // namespace
  void
  CategoryLogger::on_log_event(
      Level lvl, const llarp::util::source_location& loc, const std::string& msg) const
  {
    auto uptime = friendly_duration(llarp::uptime());
    switch (lvl)
    {
      case Level::lvl_trace:
        spdlog::trace("[{}] [{}|{}:{}] {}", uptime, m_Name, format_sl(loc), loc.line(), msg);
        return;
      case Level::lvl_debug:
        spdlog::debug("[{}] [{}|{}:{}] {}", uptime, m_Name, format_sl(loc), loc.line(), msg);
        return;
      case Level::lvl_info:
        spdlog::info("[{}] [{}|{}:{}] {}", uptime, m_Name, format_sl(loc), loc.line(), msg);
        return;
      case Level::lvl_warning:
        spdlog::warn("[{}] [{}|{}:{}] {}", uptime, m_Name, format_sl(loc), loc.line(), msg);
        return;
      case Level::lvl_error:
        spdlog::error("[{}] [{}|{}:{}] {}", uptime, m_Name, format_sl(loc), loc.line(), msg);
        return;
    }
  }

}  // namespace llarp::log
