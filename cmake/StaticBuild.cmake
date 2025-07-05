# cmake bits to do a full static build, downloading and building all dependencies.

# Most of these are CACHE STRINGs so that you can override them using -DWHATEVER during cmake
# invocation to override.

set(LOCAL_MIRROR "" CACHE STRING "local mirror path/URL for lib downloads")

set(OPENSSL_VERSION 3.0.7 CACHE STRING "openssl version")
set(OPENSSL_MIRROR ${LOCAL_MIRROR} https://www.openssl.org/source CACHE STRING "openssl download mirror(s)")
set(OPENSSL_SOURCE openssl-${OPENSSL_VERSION}.tar.gz)
set(OPENSSL_HASH SHA256=83049d042a260e696f62406ac5c08bf706fd84383f945cf21bd61e9ed95c396e
    CACHE STRING "openssl source hash")

set(EXPAT_VERSION 2.5.0 CACHE STRING "expat version")
string(REPLACE "." "_" EXPAT_TAG "R_${EXPAT_VERSION}")
set(EXPAT_MIRROR ${LOCAL_MIRROR} https://github.com/libexpat/libexpat/releases/download/${EXPAT_TAG}
    CACHE STRING "expat download mirror(s)")
set(EXPAT_SOURCE expat-${EXPAT_VERSION}.tar.xz)
set(EXPAT_HASH SHA512=2da73b991b7c0c54440485c787e5edeb3567230204e31b3cac1c3a6713ec6f9f1554d3afffc0f8336168dfd5df02db4a69bcf21b4d959723d14162d13ab87516
    CACHE STRING "expat source hash")

set(UNBOUND_VERSION 1.17.0 CACHE STRING "unbound version")
set(UNBOUND_MIRROR ${LOCAL_MIRROR} https://nlnetlabs.nl/downloads/unbound CACHE STRING "unbound download mirror(s)")
set(UNBOUND_SOURCE unbound-${UNBOUND_VERSION}.tar.gz)
set(UNBOUND_HASH SHA512=f6b9f279330fb19b5feca09524959940aad8c4e064528aa82b369c726d77e9e8e5ca23f366f6e9edcf2c061b96f482ed7a2c26ac70fc15ae5762b3d7e36a5284
    CACHE STRING "unbound source hash")

set(SQLITE3_VERSION 3400000 CACHE STRING "sqlite3 version")
set(SQLITE3_MIRROR ${LOCAL_MIRROR} https://www.sqlite.org/2022
    CACHE STRING "sqlite3 download mirror(s)")
set(SQLITE3_SOURCE sqlite-autoconf-${SQLITE3_VERSION}.tar.gz)
set(SQLITE3_HASH SHA3_256=7ee8f02b21edb4489df5082b5cf5b7ef47bcebcdb0e209bf14240db69633c878
  CACHE STRING "sqlite3 source hash")

set(SODIUM_VERSION 1.0.18 CACHE STRING "libsodium version")
set(SODIUM_MIRROR ${LOCAL_MIRROR}
  https://download.libsodium.org/libsodium/releases
  https://github.com/jedisct1/libsodium/releases/download/${SODIUM_VERSION}-RELEASE
  CACHE STRING "libsodium mirror(s)")
set(SODIUM_SOURCE libsodium-${SODIUM_VERSION}.tar.gz)
set(SODIUM_HASH SHA512=17e8638e46d8f6f7d024fe5559eccf2b8baf23e143fadd472a7d29d228b186d86686a5e6920385fe2020729119a5f12f989c3a782afbd05a8db4819bb18666ef
  CACHE STRING "libsodium source hash")

set(ZMQ_VERSION 4.3.4 CACHE STRING "libzmq version")
set(ZMQ_MIRROR ${LOCAL_MIRROR} https://github.com/zeromq/libzmq/releases/download/v${ZMQ_VERSION}
    CACHE STRING "libzmq mirror(s)")
set(ZMQ_SOURCE zeromq-${ZMQ_VERSION}.tar.gz)
set(ZMQ_HASH SHA512=e198ef9f82d392754caadd547537666d4fba0afd7d027749b3adae450516bcf284d241d4616cad3cb4ad9af8c10373d456de92dc6d115b037941659f141e7c0e
    CACHE STRING "libzmq source hash")

set(LIBUV_VERSION 1.44.2 CACHE STRING "libuv version")
set(LIBUV_MIRROR ${LOCAL_MIRROR} https://dist.libuv.org/dist/v${LIBUV_VERSION}
    CACHE STRING "libuv mirror(s)")
set(LIBUV_SOURCE libuv-v${LIBUV_VERSION}.tar.gz)
set(LIBUV_HASH SHA512=91197ff9303112567bbb915bbb88058050e2ad1c048815a3b57c054635d5dc7df458b956089d785475290132236cb0edcfae830f5d749de29a9a3213eeaf0b20
    CACHE STRING "libuv source hash")

set(ZLIB_VERSION 1.2.13 CACHE STRING "zlib version")
set(ZLIB_MIRROR ${LOCAL_MIRROR} https://zlib.net
    CACHE STRING "zlib mirror(s)")
set(ZLIB_SOURCE zlib-${ZLIB_VERSION}.tar.xz)
set(ZLIB_HASH SHA256=d14c38e313afc35a9a8760dadf26042f51ea0f5d154b0630a31da0540107fb98
  CACHE STRING "zlib source hash")

set(CURL_VERSION 7.86.0 CACHE STRING "curl version")
set(CURL_MIRROR ${LOCAL_MIRROR} https://curl.haxx.se/download https://curl.askapache.com
  CACHE STRING "curl mirror(s)")
set(CURL_SOURCE curl-${CURL_VERSION}.tar.xz)
set(CURL_HASH SHA512=18e03a3c00f22125e07bddb18becbf5acdca22baeb7b29f45ef189a5c56f95b2d51247813f7a9a90f04eb051739e9aa7d3a1c5be397bae75d763a2b918d1b656
  CACHE STRING "curl source hash")

include(ExternalProject)

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

build_external(libuv
  CONFIGURE_COMMAND ./autogen.sh && ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --with-pic --disable-shared --enable-static "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
  BUILD_BYPRODUCTS
    ${DEPS_DESTDIR}/lib/libuv.a
    ${DEPS_DESTDIR}/include/uv.h
  )
add_static_target(libuv libuv_external libuv.a)
target_link_libraries(libuv INTERFACE ${CMAKE_DL_LIBS})


build_external(zlib
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS} -fPIC" ${cross_extra} ./configure --prefix=${DEPS_DESTDIR} --static
  BUILD_BYPRODUCTS
    ${DEPS_DESTDIR}/lib/libz.a
    ${DEPS_DESTDIR}/include/zlib.h
)
add_static_target(zlib zlib_external libz.a)


set(openssl_system_env "")
set(openssl_arch "")
set(openssl_configure_command ./config)
set(openssl_flags "CFLAGS=${deps_CFLAGS}")
set(unbound_ldflags "")
if(CMAKE_CROSSCOMPILING)
  if(ANDROID)
    set(openssl_arch android-${android_machine})
    set(openssl_system_env LD=${deps_ld} RANLIB=${deps_ranlib} AR=${deps_ar} ANDROID_NDK_ROOT=${CMAKE_ANDROID_NDK} "PATH=${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin:$ENV{PATH}")
    list(APPEND openssl_flags "CPPFLAGS=-D__ANDROID_API__=${ANDROID_API}")
    set(openssl_extra_opts no-asm)
  elseif(ARCH_TRIPLET STREQUAL mips64-linux-gnuabi64)
    set(openssl_arch linux-mips64)
  elseif(ARCH_TRIPLET STREQUAL mips-linux-gnu)
    set(openssl_arch linux-mips32)
  elseif(ARCH_TRIPLET STREQUAL mips-openwrt-linux)
    set(unbound_ldflags "-latomic")
    set(openssl_arch linux-mips32)
  elseif(ARCH_TRIPLET STREQUAL mipsel-linux-gnu)
    set(openssl_arch linux-mips)
  elseif(ARCH_TRIPLET STREQUAL aarch64-linux-gnu OR ARCH_TRIPLET STREQUAL aarch64-openwrt-linux-musl)
    # cross compile arm64
    set(openssl_arch linux-aarch64)
  elseif(ARCH_TRIPLET MATCHES arm-linux)
    # cross compile armhf
    set(openssl_arch linux-armv4)
  elseif(ARCH_TRIPLET MATCHES powerpc64le)
    # cross compile ppc64le
    set(openssl_arch linux-ppc64le)
  endif()
elseif(CMAKE_C_FLAGS MATCHES "-march=armv7")
  # Help openssl figure out that we're building from armv7 even if on armv8 hardware:
  set(openssl_arch linux-armv4)
endif()


build_external(openssl
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=${deps_cc} ${openssl_system_env} ${openssl_configure_command}
    --prefix=${DEPS_DESTDIR} --libdir=lib ${openssl_extra_opts}
    no-shared no-capieng no-dso no-dtls1 no-ec_nistp_64_gcc_128 no-gost
    no-md2 no-rc5 no-rdrand no-rfc3779 no-sctp no-ssl-trace no-ssl3
    no-static-engine no-tests no-weak-ssl-ciphers no-zlib no-zlib-dynamic ${openssl_flags}
    ${openssl_arch}
  BUILD_COMMAND ${CMAKE_COMMAND} -E env ${openssl_system_env} ${_make}
  INSTALL_COMMAND ${_make} install_sw
  BUILD_BYPRODUCTS
    ${DEPS_DESTDIR}/lib/libssl.a ${DEPS_DESTDIR}/lib/libcrypto.a
    ${DEPS_DESTDIR}/include/openssl/ssl.h ${DEPS_DESTDIR}/include/openssl/crypto.h
)
add_static_target(OpenSSL::SSL openssl_external libssl.a)
add_static_target(OpenSSL::Crypto openssl_external libcrypto.a)

set(OPENSSL_INCLUDE_DIR ${DEPS_DESTDIR}/include)
set(OPENSSL_CRYPTO_LIBRARY ${DEPS_DESTDIR}/lib/libcrypto.a ${DEPS_DESTDIR}/lib/libssl.a)
set(OPENSSL_ROOT_DIR ${DEPS_DESTDIR})

build_external(expat
  CONFIGURE_COMMAND ./configure ${cross_host} --prefix=${DEPS_DESTDIR} --enable-static
  --disable-shared --with-pic --without-examples --without-tests --without-docbook --without-xmlwf
  "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
)
add_static_target(expat expat_external libexpat.a)


build_external(unbound
  DEPENDS openssl_external expat_external
  ${unbound_patch}
  CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared
  --enable-static --with-libunbound-only --with-pic
  --$<IF:$<BOOL:${WITH_LTO}>,enable,disable>-flto --with-ssl=${DEPS_DESTDIR}
  --with-libexpat=${DEPS_DESTDIR}
  "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}" "LDFLAGS=${unbound_ldflags}"
)
add_static_target(libunbound unbound_external libunbound.a)
set_target_properties(libunbound PROPERTIES INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto")



build_external(sodium CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared
          --enable-static --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}")
add_static_target(sodium sodium_external libsodium.a)
