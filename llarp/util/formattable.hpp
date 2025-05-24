#pragma once

#include <fmt/format.h>
#include <spdlog/common.h>
#include <type_traits>
#include <source_location>
#include <string_view>

// Formattable types can specialize this to true and will get automatic fmt formattering support via
// their .ToString() method.

namespace llarp
{
  // Types can opt-in to being formatting via .ToString() by specializing this to true.  This also
  // allows scoped enums by instead looking for a call to `ToString(val)` (and so there should be a
  // ToString function in the same namespace as the scoped enum to pick it up via ADL).
  template <typename T>
  constexpr bool IsToStringFormattable = false;

  // e.g.:
  // template <> inline constexpr bool IsToStringFormattable<MyType> = true;

#ifdef __cpp_lib_is_scoped_enum
  using std::is_scoped_enum;
  using std::is_scoped_enum_v;
#else
  template <typename T, bool = std::is_enum_v<T>>
  struct is_scoped_enum : std::false_type
  {};

  template <typename T>
  struct is_scoped_enum<T, true>
      : std::bool_constant<!std::is_convertible_v<T, std::underlying_type_t<T>>>
  {};

  template <typename T>
  constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;
#endif

}  // namespace llarp

#if !defined(USE_GHC_FILESYSTEM) && FMT_VERSION >= 80102

// Native support in fmt added after fmt 8.1.1
#include <fmt/std.h>

#else

#include <llarp/util/fs.hpp>

namespace fmt
{
  template <>
  struct formatter<fs::path> : formatter<std::string_view>
  {
    template <typename FormatContext>
    auto
    format(const fs::path& p, FormatContext& ctx) const
    {
      return formatter<std::string_view>::format(p.string(), ctx);
    }
  };
}  // namespace fmt

#endif

namespace
{

  inline auto
  format_sl(const std::source_location& loc)
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

    return spdlog::source_loc{filename.data(), static_cast<int>(loc.line()), loc.function_name()};
  }
}  // namespace

namespace fmt
{

  template <typename T>
  struct formatter<T, char, std::enable_if_t<llarp::IsToStringFormattable<T>>>
      : formatter<std::string_view>
  {
    template <typename FormatContext>
    auto
    format(const T& val, FormatContext& ctx) const
    {
      if constexpr (llarp::is_scoped_enum_v<T>)
        return formatter<std::string_view>::format(ToString(val), ctx);
      else
        return formatter<std::string_view>::format(val.ToString(), ctx);
    }
  };

}  // namespace fmt
