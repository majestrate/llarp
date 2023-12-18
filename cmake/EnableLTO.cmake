# -flto
include(CheckIPOSupported)
option(WITH_LTO "enable lto on compile time" ON)

if(WITH_LTO)
  if(WIN32)
    message(FATAL_ERROR "LTO not supported on win32 targets, please set -DWITH_LTO=OFF")
  endif()
  check_ipo_supported(RESULT IPO_ENABLED OUTPUT ipo_error)
  if(IPO_ENABLED)
    message(STATUS "LTO enabled")
  else()
    message(WARNING "LTO not supported by compiler: ${ipo_error}")
  endif()
else()
  message(STATUS "LTO disabled")
  set(IPO_ENABLED OFF)
endif()

function(enable_lto)
  if(IPO_ENABLED)
    set_target_properties(${ARGN} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endfunction()