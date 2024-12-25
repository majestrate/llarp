#pragma once

/// namespace for platform feature detection constexprs
namespace llarp::platform
{
  ///  are we linux  ?
  inline constexpr bool is_linux =
#ifdef __linux__
      true
#else
      false
#endif
      ;

  /// building with systemd enabled ?
  inline constexpr bool with_systemd =
#ifdef WITH_SYSTEMD
      true
#else
      false
#endif
      ;

  ///  are we freebsd ?
  inline constexpr bool is_freebsd =
#ifdef __FreeBSD__
      true
#else
      false
#endif
      ;

  /// are we an android platform ?
  inline constexpr bool is_android =
#ifdef ANDROID
      true
#else
      false
#endif
      ;

  /// are we running with pybind simulation mode enabled?
  inline constexpr bool is_simulation =
#ifdef LOKINET_HIVE
      true
#else
      false
#endif
      ;
  /// do we have systemd support ?
  // on cross compiles sometimes weird permutations of target and host make this value not correct,
  // this ensures it always is
  inline constexpr bool has_systemd = is_linux and with_systemd and not(is_android);

  /// are we a mobile phone ?
  inline constexpr bool is_mobile = is_android;

  /// does this platform support native ipv6 ?
  inline constexpr bool supports_ipv6 = true;
}  // namespace llarp::platform
