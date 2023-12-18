
# all of our dependencies are set up in here

find_package(PkgConfig REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(base_libs INTERFACE Threads::Threads)

# TODO: if linux else if nop 
pkg_check_modules(SD libsystemd IMPORTED_TARGET)
# Default WITH_SYSTEMD to true if we found it
option(WITH_SYSTEMD "enable systemd integration for sd_notify" ${SD_FOUND})


if(NOT TARGET sodium)
  pkg_check_modules(SODIUM libsodium>=1.0.18 IMPORTED_TARGET REQUIRED)
  add_library(sodium INTERFACE)
  if(NOT SODIUM_FOUND)
    message(FATAL_ERROR "libsodium not found")
  endif()
  # Need this target export so that loki-mq properly picks up sodium
  export(TARGETS sodium NAMESPACE sodium:: FILE sodium-exports.cmake)
endif()


if(WITH_SYSTEMD AND LINUX)
  if(NOT SD_FOUND)
    message(FATAL_ERROR "libsystemd not found")
  endif()
  target_link_libraries(base_libs INTERFACE PkgConfig::SD)
  target_compile_definitions(base_libs INTERFACE WITH_SYSTEMD)
endif()

pkg_check_modules(EVENT libevent REQUIRED)
target_link_libraries(base_libs INTERFACE PkgConfig:EVENT)

pkg_check_modules(SPDLOG spdlog>=1.10 REQUIRED)
target_link_libraries(base_libs INTERFACE PkgConfig:SPDLOG)

