set(DEPS_DESTDIR ${CMAKE_BINARY_DIR}/static-deps)
set(DEPS_SOURCEDIR ${CMAKE_BINARY_DIR}/static-deps-sources)

include_directories(BEFORE SYSTEM ${DEPS_DESTDIR}/include)

file(MAKE_DIRECTORY ${DEPS_DESTDIR}/include)


set(deps_cc "${CMAKE_C_COMPILER}")
set(deps_cxx "${CMAKE_CXX_COMPILER}")
if(CMAKE_C_COMPILER_LAUNCHER)
    set(deps_cc "${CMAKE_C_COMPILER_LAUNCHER} ${deps_cc}")
endif()
if(CMAKE_CXX_COMPILER_LAUNCHER)
    set(deps_cxx "${CMAKE_CXX_COMPILER_LAUNCHER} ${deps_cxx}")
endif()

set(cross_host "")
set(cross_rc "")
if(CMAKE_CROSSCOMPILING)
    set(cross_host "--host=${ARCH_TRIPLET}")
endif()

if(ANDROID)
    set(android_toolchain_suffix linux-android)
    set(android_compiler_suffix linux-android23)
    if(CMAKE_ANDROID_ARCH_ABI MATCHES x86_64)
        set(android_machine x86_64)
        set(cross_host "--host=x86_64-linux-android")
        set(android_compiler_prefix x86_64)
        set(android_compiler_suffix linux-android23)
        set(android_toolchain_prefix x86_64)
        set(android_toolchain_suffix linux-android)
    elseif(CMAKE_ANDROID_ARCH_ABI MATCHES x86)
        set(android_machine x86)
        set(cross_host "--host=i686-linux-android")
        set(android_compiler_prefix i686)
        set(android_compiler_suffix linux-android23)
        set(android_toolchain_prefix i686)
        set(android_toolchain_suffix linux-android)
    elseif(CMAKE_ANDROID_ARCH_ABI MATCHES armeabi-v7a)
        set(android_machine arm)
        set(cross_host "--host=armv7a-linux-androideabi")
        set(android_compiler_prefix armv7a)
        set(android_compiler_suffix linux-androideabi23)
        set(android_toolchain_prefix arm)
        set(android_toolchain_suffix linux-androideabi)
    elseif(CMAKE_ANDROID_ARCH_ABI MATCHES arm64-v8a)
        set(android_machine arm64)
        set(cross_host "--host=aarch64-linux-android")
        set(android_compiler_prefix aarch64)
        set(android_compiler_suffix linux-android23)
        set(android_toolchain_prefix aarch64)
        set(android_toolchain_suffix linux-android)
    else()
        message(FATAL_ERROR "unknown android arch: ${CMAKE_ANDROID_ARCH_ABI}")
    endif()
    set(deps_cc "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_compiler_suffix}-clang")
    set(deps_cxx "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_compiler_suffix}-clang++")
    set(deps_ld "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_compiler_prefix}-${android_toolchain_suffix}-ld")
    set(deps_ranlib "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_toolchain_prefix}-${android_toolchain_suffix}-ranlib")
    set(deps_ar "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin/${android_toolchain_prefix}-${android_toolchain_suffix}-ar")
endif()

set(deps_CFLAGS "-O2")
set(deps_CXXFLAGS "-O2")

if(WITH_LTO)
    set(deps_CFLAGS "${deps_CFLAGS} -flto")
endif()


if("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
    set(_make $(MAKE))
else()
    set(_make make)
endif()


# Builds a target; takes the target name (e.g. "readline") and builds it in an external project with
# target name suffixed with `_external`.  Its upper-case value is used to get the download details
# (from the variables set above).  The following options are supported and passed through to
# ExternalProject_Add if specified.  If omitted, these defaults are used:
set(build_def_DEPENDS "")
set(build_def_PATCH_COMMAND "")
set(build_def_CONFIGURE_COMMAND ./configure ${cross_host} --disable-shared --prefix=${DEPS_DESTDIR} --with-pic
        "CC=${deps_cc}" "CXX=${deps_cxx}" "CFLAGS=${deps_CFLAGS}" "CXXFLAGS=${deps_CXXFLAGS}" ${cross_rc})
set(build_def_BUILD_COMMAND ${_make})
set(build_def_INSTALL_COMMAND ${_make} install)
set(build_def_BUILD_BYPRODUCTS ${DEPS_DESTDIR}/lib/lib___TARGET___.a ${DEPS_DESTDIR}/include/___TARGET___.h)

function(expand_urls output source_file)
    set(expanded)
    foreach(mirror ${ARGN})
        list(APPEND expanded "${mirror}/${source_file}")
    endforeach()
    set(${output} "${expanded}" PARENT_SCOPE)
endfunction()

function(add_static_target target ext_target libname)
    add_library(${target} STATIC IMPORTED GLOBAL)
    add_dependencies(${target} ${ext_target})
    set_target_properties(${target} PROPERTIES
            IMPORTED_LOCATION ${DEPS_DESTDIR}/lib/${libname}
    )
endfunction()

function(build_external target)
    set(options DEPENDS PATCH_COMMAND CONFIGURE_COMMAND BUILD_COMMAND INSTALL_COMMAND BUILD_BYPRODUCTS)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${options}")
    foreach(o ${options})
        if(NOT DEFINED arg_${o})
            set(arg_${o} ${build_def_${o}})
        endif()
    endforeach()
    string(REPLACE ___TARGET___ ${target} arg_BUILD_BYPRODUCTS "${arg_BUILD_BYPRODUCTS}")

    string(TOUPPER "${target}" prefix)
    expand_urls(urls ${${prefix}_SOURCE} ${${prefix}_MIRROR})
    ExternalProject_Add("${target}_external"
            DEPENDS ${arg_DEPENDS}
            BUILD_IN_SOURCE ON
            PREFIX ${DEPS_SOURCEDIR}
            URL ${urls}
            URL_HASH ${${prefix}_HASH}
            DOWNLOAD_NO_PROGRESS ON
            PATCH_COMMAND ${arg_PATCH_COMMAND}
            CONFIGURE_COMMAND ${arg_CONFIGURE_COMMAND}
            BUILD_COMMAND ${arg_BUILD_COMMAND}
            INSTALL_COMMAND ${arg_INSTALL_COMMAND}
            BUILD_BYPRODUCTS ${arg_BUILD_BYPRODUCTS}
    )
endfunction()


function(build_external_module target)
    set(options DEPENDS PATCH_COMMAND CONFIGURE_COMMAND BUILD_COMMAND INSTALL_COMMAND BUILD_BYPRODUCTS)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "${options}")
    foreach(o ${options})
        if(NOT DEFINED arg_${o})
            set(arg_${o} ${build_def_${o}})
        endif()
    endforeach()
    string(REPLACE ___TARGET___ ${target} arg_BUILD_BYPRODUCTS "${arg_BUILD_BYPRODUCTS}")

    string(TOUPPER "${target}" prefix)
    ExternalProject_Add("${target}_submodule"
            DEPENDS ${arg_DEPENDS}
            BUILD_IN_SOURCE ON
            PREFIX ${CMAKE_CURRENT_BINARY_DIR}
            URL_HASH ${${prefix}_HASH}
            DOWNLOAD_NO_PROGRESS ON
            GIT_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/${target}
            PATCH_COMMAND ${arg_PATCH_COMMAND}
            CONFIGURE_COMMAND ${arg_CONFIGURE_COMMAND}
            BUILD_COMMAND ${arg_BUILD_COMMAND}
            INSTALL_COMMAND ${arg_INSTALL_COMMAND}
            BUILD_BYPRODUCTS ${arg_BUILD_BYPRODUCTS}
    )
endfunction()

function(add_static_module_target target ext_target libname)
    add_library(${target} STATIC IMPORTED GLOBAL)
    add_dependencies(${target} ${ext_target})
    set_target_properties(${target} PROPERTIES
            IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib/${libname})
endfunction()