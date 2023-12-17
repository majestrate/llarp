# checks if we are running on a supported platform
include(CheckCXXSourceCompiles)
include(CheckLibraryExists)



if(EMBEDDED_CFG OR ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  link_libatomic()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  add_definitions(-D_BSD_SOURCE)
  add_definitions(-D_GNU_SOURCE)
  add_definitions(-D_XOPEN_SOURCE=700)
endif()
