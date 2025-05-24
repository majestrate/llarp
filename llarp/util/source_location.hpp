#pragma once

#if __cplusplus >= 202002L && __has_include(<source_location>)
#include <source_location>
namespace llarp::util
{
  using source_location = std::source_location;
}
#define HAS_SOURCE_LOCATION
#elif defined(__has_builtin)
// GCC and Clang builtin support
#if __has_builtin(__builtin_FILE) && __has_builtin(__builtin_LINE) \
    && __has_builtin(__builtin_FUNCTION) && __has_builtin(__builtin_COLUMN)
#define SOURCE_LOCATION_BUILTIN_SUPPORT
#endif
#endif

#ifndef SOURCE_LOCATION_BUILTIN_SUPPORT
// Fallback for compilers without builtin support
#define SOURCE_LOCATION_FALLBACK
#endif

#ifndef HAS_SOURCE_LOCATION
namespace llarp::util
{
  class source_location
  {
   public:
    constexpr source_location() noexcept = default;
    constexpr source_location(const source_location&) noexcept = default;

#ifdef SOURCE_LOCATION_BUILTIN_SUPPORT
    static constexpr source_location
    current(
        const char* file = __builtin_FILE(),
        const char* function = __builtin_FUNCTION(),
        std::uint_least32_t line = __builtin_LINE(),
        std::uint_least32_t column = __builtin_COLUMN()) noexcept
    {
      source_location loc{};
      loc.file_ = file;
      loc.function_ = function;
      loc.line_ = line;
      loc.column_ = column;
      return loc;
    }
#else
    static constexpr source_location
    current(
        const char* file = __FILE__,
        const char* function =
#if defined(__GNUC__) || defined(__clang__)
            __PRETTY_FUNCTION__,
#elif defined(_MSC_VER)
            __FUNCSIG__,
#else
            __func__,
#endif
        std::uint_least32_t line = __LINE__,
        std::uint_least32_t column = 0) noexcept
    {
      source_location loc{};
      loc.file_ = file;
      loc.function_ = function;
      loc.line_ = line;
      loc.column_ = column;
      return loc;
    }
#endif

    // Accessors
    constexpr const char*
    file_name() const noexcept
    {
      return file_;
    }

    constexpr const char*
    function_name() const noexcept
    {
      return function_;
    }

    constexpr std::uint_least32_t
    line() const noexcept
    {
      return line_;
    }

    constexpr std::uint_least32_t
    column() const noexcept
    {
      return column_;
    }

   private:
    const char* file_ = "unknown";
    const char* function_ = "unknown";
    std::uint_least32_t line_ = 0;
    std::uint_least32_t column_ = 0;
  };

}  // namespace llarp::util
#endif
