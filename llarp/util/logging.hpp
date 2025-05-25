#pragma once

// Header for making actual log statements such as llarp::log::Info and so on work.

#include <string>
#include <string_view>
#include <array>
#include <fmt/format.h>
#include "source_location.hpp"
#include "time.hpp"
#include <memory>

namespace llarp::log
{

  enum class Level : int
  {
    lvl_trace = 0,
    lvl_debug = 1,
    lvl_info = 2,
    lvl_warning = 3,
    lvl_error = 4,
    off = 99
  };

  Level
  level_from_string(std::string_view str);

  class CategoryLogger
  {
    Level m_MinLevel;
    std::string m_Name;

   public:
    CategoryLogger(std::string_view name);

    void
    min_level(Level lvl);

    bool
    should_log(Level lvl) const;

    void
    on_log_event(Level lvl, const llarp::util::source_location& loc, const std::string& msg) const;
  };

  using CategoryLogger_ptr = std::shared_ptr<CategoryLogger>;

  namespace detail
  {
    template <typename... TArgs>
    std::string
    make_message(fmt::format_string<TArgs...> format, TArgs&&... args)
    {
      return fmt::format(format, std::forward<TArgs>(args)...);
    }

  }  // namespace detail

  template <typename... T>
  struct trace
  {
    trace(
        const CategoryLogger_ptr& cat_logger,
        fmt::format_string<T...> format,
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
    {
      if (not cat_logger)
        return;
      if (cat_logger->should_log(Level::lvl_trace))
        cat_logger->on_log_event(
            Level::lvl_trace, location, detail::make_message(format, std::forward<T>(args)...));
    }
  };

  template <typename... T>
  struct debug
  {
    debug(
        const CategoryLogger_ptr& cat_logger,
        fmt::format_string<T...> format,
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
    {
      if (not cat_logger)
        return;
      if (cat_logger->should_log(Level::lvl_debug))
        cat_logger->on_log_event(
            Level::lvl_debug, location, detail::make_message(format, std::forward<T>(args)...));
    }
  };

  template <typename... T>
  struct info
  {
    info(
        const CategoryLogger_ptr& cat_logger,
        fmt::format_string<T...> format,
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
    {
      if (not cat_logger)
        return;
      if (cat_logger->should_log(Level::lvl_info))
        cat_logger->on_log_event(
            Level::lvl_info, location, detail::make_message(format, std::forward<T>(args)...));
    }
  };

  template <typename... T>
  struct warning
  {
    warning(
        const CategoryLogger_ptr& cat_logger,
        fmt::format_string<T...> format,
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
    {
      if (not cat_logger)
        return;
      if (cat_logger->should_log(Level::lvl_warning))
        cat_logger->on_log_event(
            Level::lvl_warning, location, detail::make_message(format, std::forward<T>(args)...));
    }
  };

  template <typename... T>
  struct error
  {
    error(
        const CategoryLogger_ptr& cat_logger,
        fmt::format_string<T...> format,
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
    {
      if (not cat_logger)
        return;
      if (cat_logger->should_log(Level::lvl_error))
        cat_logger->on_log_event(
            Level::lvl_error, location, detail::make_message(format, std::forward<T>(args)...));
    }
  };

  template <typename... T>
  trace(const CategoryLogger_ptr& cat, fmt::format_string<T...> fmt, T&&... args) -> trace<T...>;

  template <typename... T>
  debug(const CategoryLogger_ptr& cat, fmt::format_string<T...> fmt, T&&... args) -> debug<T...>;

  template <typename... T>
  info(const CategoryLogger_ptr& cat, fmt::format_string<T...> fmt, T&&... args) -> info<T...>;

  template <typename... T>
  warning(const CategoryLogger_ptr& cat, fmt::format_string<T...> fmt, T&&... args)
      -> warning<T...>;

  template <typename... T>
  error(const CategoryLogger_ptr& cat, fmt::format_string<T...> fmt, T&&... args) -> error<T...>;

  CategoryLogger_ptr
  Cat(std::string_view name);

  void
  set_log_level(Level lvl);
}  // namespace llarp::log

// Not ready to pollute these deprecation warnings everywhere yet
#if 0
#define LOKINET_LOG_DEPRECATED(Meth) \
  [[deprecated("Use formatted log::" #Meth "(cat, fmt, args...) instead")]]
#else
#define LOKINET_LOG_DEPRECATED(Meth)
#endif

// Deprecated loggers (in the top-level llarp namespace):
namespace llarp
{

  namespace log_detail
  {
    inline log::CategoryLogger_ptr legacy_logger = log::Cat("llarp");

    template <typename>
    struct concat_args_fmt_impl;
    template <size_t... I>
    struct concat_args_fmt_impl<std::integer_sequence<size_t, I...>>
    {
      constexpr static std::array<char, sizeof...(I)> format{(I % 2 == 0 ? '{' : '}')...};
    };
    template <size_t N>
    constexpr std::string_view
    concat_args_fmt()
    {
      return std::string_view{
          concat_args_fmt_impl<std::make_index_sequence<2 * N>>::format.data(), 2 * N};
    }
  }  // namespace log_detail

  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Trace) LogTrace : log::trace<T...>
  {
    LogTrace(
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
        : log::trace<T...>{
            log_detail::legacy_logger,
            log_detail::concat_args_fmt<sizeof...(T)>(),
            std::forward<T>(args)...,
            location}
    {}
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Debug) LogDebug : log::debug<T...>
  {
    LogDebug(
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
        : log::debug<T...>{
            log_detail::legacy_logger,
            log_detail::concat_args_fmt<sizeof...(T)>(),
            std::forward<T>(args)...,
            location}
    {}
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Info) LogInfo : log::info<T...>
  {
    LogInfo(
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
        : log::info<T...>{
            log_detail::legacy_logger,
            log_detail::concat_args_fmt<sizeof...(T)>(),
            std::forward<T>(args)...,
            location}
    {}
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Warning) LogWarn : log::warning<T...>
  {
    LogWarn(
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
        : log::warning<T...>{
            log_detail::legacy_logger,
            log_detail::concat_args_fmt<sizeof...(T)>(),
            std::forward<T>(args)...,
            location}
    {}
  };
  template <typename... T>
  struct LOKINET_LOG_DEPRECATED(Error) LogError : log::error<T...>
  {
    LogError(
        T&&... args,
        const llarp::util::source_location& location = llarp::util::source_location::current())
        : log::error<T...>{
            log_detail::legacy_logger,
            log_detail::concat_args_fmt<sizeof...(T)>(),
            std::forward<T>(args)...,
            location}
    {}
  };

  template <typename... T>
  LogTrace(T&&...) -> LogTrace<T...>;

  template <typename... T>
  LogDebug(T&&...) -> LogDebug<T...>;

  template <typename... T>
  LogInfo(T&&...) -> LogInfo<T...>;

  template <typename... T>
  LogWarn(T&&...) -> LogWarn<T...>;

  template <typename... T>
  LogError(T&&...) -> LogError<T...>;

}  // namespace llarp
