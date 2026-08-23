option(SUBMODULE_CHECK "Enables checking that vendored library submodules are up to date" ON)
if(SUBMODULE_CHECK)
  find_package(Git)
  if(GIT_FOUND)
    function(check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:external/${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if (upToDate)
        message(STATUS "Submodule 'external/${relative_path}' is up-to-date")
      else()
        message(FATAL_ERROR "Submodule 'external/${relative_path}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
      endif()

      # Extra arguments check nested submodules
      foreach(submod ${ARGN})
        execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path}/${submod} OUTPUT_VARIABLE localHead)
        execute_process(COMMAND git rev-parse "HEAD:${submod}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE checkedHead)
        string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
        if (NOT upToDate)
            message(FATAL_ERROR "Nested submodule '${relative_path}/${submod}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
        endif()
      endforeach()
    endfunction ()

    message(STATUS "Checking submodules")
    check_submodule(oxen-encoding)
    check_submodule(uvw)
    check_submodule(CLI11)
    check_submodule(fmt)
    check_submodule(spdlog)
  endif()
endif()

macro(system_or_submodule BIGNAME smallname pkgconf subdir)
  option(FORCE_${BIGNAME}_SUBMODULE "force using ${smallname} submodule" OFF)
  if(NOT BUILD_STATIC_DEPS AND NOT FORCE_${BIGNAME}_SUBMODULE AND NOT FORCE_ALL_SUBMODULES)
    pkg_check_modules(${BIGNAME} ${pkgconf} IMPORTED_TARGET)
  endif()
  if(${BIGNAME}_FOUND)
    add_library(${smallname} INTERFACE)
    if(NOT TARGET PkgConfig::${BIGNAME} AND CMAKE_VERSION VERSION_LESS "3.21")
      # Work around cmake bug 22180 (PkgConfig::THING not set if no flags needed)
    else()
      target_link_libraries(${smallname} INTERFACE PkgConfig::${BIGNAME})
    endif()
    message(STATUS "Found system ${smallname} ${${BIGNAME}_VERSION}")
  else()
    message(STATUS "using ${smallname} submodule")
    add_subdirectory(${subdir} EXCLUDE_FROM_ALL)
  endif()
  if(NOT TARGET ${smallname}::${smallname})
    add_library(${smallname}::${smallname} ALIAS ${smallname})
  endif()
endmacro()

include(ExternalProject)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/build_module.cmake)

set(sctp_prefix ${CMAKE_CURRENT_BINARY_DIR}/sctplib)

add_library(sctplib INTERFACE)
build_external_module(sctplib
        CONFIGURE_COMMAND ./autogen.sh && ./configure ${cross_host} ${cross_rc} --prefix=${sctp_prefix} --disable-shared --enable-static --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
        BUILD_BYPRODUCTS
        ${sctp_prefix}/lib/libsctplib.a
        ${sctp_prefix}/include/netinet/sctp.h
)
add_static_module_target(sctplib_impl sctplib_submodule libsctplib.a)
target_link_libraries(sctplib INTERFACE sctplib_impl)
target_include_directories(sctplib INTERFACE ${sctp_prefix}/include)

system_or_submodule(FMT fmt libfmt>=12.1.0 fmt)
system_or_submodule(SPDLOG spdlog libspdlog>=1.17.0 spdlog)
system_or_submodule(OXENC oxenc liboxenc>=1.1.0 oxen-encoding)

if(WITH_HIVE)
  add_subdirectory(pybind11 EXCLUDE_FROM_ALL)
endif()

system_or_submodule(CLI11 CLI11 CLI11>=2.6.2 CLI11)

if(WITH_LIBUV)
  set(USE_LIBCPP OFF CACHE INTERNAL "")
  add_library(uvw INTERFACE)
  target_include_directories(uvw INTERFACE uvw/src)
  target_link_libraries(uvw INTERFACE libuv)
endif()

# libcrypt defaults, only on with macos and non static linux
set(default_libcrypt OFF)

if(LINUX AND NOT STATIC_LINK)
  set(default_libcrypt ON)
endif()
if(MACOS)
  set(default_libcrypt ON)
endif()

option(WITH_LIBCRYPT "enable fast password hash with libcrypt" ${default_libcrypt})

add_library(lokinet-libcrypt INTERFACE)
if(WITH_LIBCRYPT)
  pkg_check_modules(LIBCRYPT libcrypt IMPORTED_TARGET REQUIRED)
  add_definitions(-DHAVE_CRYPT)
  target_link_libraries(lokinet-libcrypt INTERFACE PkgConfig::LIBCRYPT)
  message(STATUS "using libcrypt ${LIBCRYPT_VERSION}")
else()
  # TODO static build lib crypt?
  message(STATUS "not building with libcrypt")
endif()
