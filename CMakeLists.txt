cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})


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

include(CheckCXXSourceCompiles)
# set cxx standards
include(OurCxxStandards)

# make sure our cxx toolchain is sane 
include(OurCxxFeatures)
include(OurCxxCompileFlags)
include(OurCxxSanitizers)
include(OurCxxCoverage)

# option()s for BUILD_*
include(OurBuildOptions)

include(GenVersion)
include(TargetLinkLibrariesSystem)
include(AddImportLibrary)
include(Atomic)
include(EnableLTO)



# base interface target where we set up global link libraries, definitions, includes, etc.
add_library(base_libs INTERFACE)
# std filesystem 
add_library(filesystem INTERFACE)

# add dependencies 
add_subdirectory(external)
# add main source
add_subdirectory(llarp)

if(BUILD_EXTRA_PLATFORMS)
  add_subdirectory(platforms)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_DOCS)
  add_subdirectory(docs)
endif()

