cmake_minimum_required(VERSION 3.24) 

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)



set(LANGS C CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  foreach(lang ${LANGS})
    if(NOT DEFINED CMAKE_${lang}_COMPILER_LAUNCHER AND NOT CMAKE_${lang}_COMPILER MATCHES ".*/ccache")
      message(STATUS "Enabling ccache for ${lang}")
      set(CMAKE_${lang}_COMPILER_LAUNCHER ${CCACHE_PROGRAM} CACHE STRING "")
    endif()
  endforeach()
endif()


project(llarp
    VERSION 0.10.0
    DESCRIPTION "llarp - IP packet onion router"
    LANGUAGES ${LANGS})

# VERY important, probably.
set(RELEASE_MOTTO "owo" CACHE STRING "Release motto")

# set up cmake extras
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()


# No in-source building
include(MacroEnsureOutOfSourceBuild)
macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out-of-source build.  Create a build directory and run 'cmake ${CMAKE_SOURCE_DIR} [options]'.")

# set cxx standards
include(OurCxxStandards)

# make sure our cxx toolchain is sane 
include(OurCxxFeatures)
include(OurCxxCompileFlags)
include(OurCxxSanitizers)
include(OurCxxCoverage)

include(GenVersion)
include(TargetLinkLibrariesSystem)
include(AddImportLibrary)
include(Atomic)


# base interface target where we set up global link libraries, definitions, includes, etc.
add_library(base_libs INTERFACE)
# std filesystem 
add_library(filesystem INTERFACE)

# add all our deps 
include(Deps)
# package maintainer and vendor quarks
include(VendorOpts)


add_subdirectory(external)
add_subdirectory(llarp)
add_subdirectory(platforms)


if(BUILD_TESTS)
  add_subdirectory(test)
endif()
if(BUILD_DOCS)
  add_subdirectory(docs)
endif()


# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()
