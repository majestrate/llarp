# cmake bits to do a full static build, downloading and building all dependencies.

# Most of these are CACHE STRINGs so that you can override them using -DWHATEVER during cmake
# invocation to override.

set(LOCAL_MIRROR "" CACHE STRING "local mirror path/URL for lib downloads")

set(OPENSSL_VERSION 3.0.19 CACHE STRING "openssl version")
set(OPENSSL_MIRROR ${LOCAL_MIRROR} https://www.openssl.org/source CACHE STRING "openssl download mirror(s)")
set(OPENSSL_SOURCE openssl-${OPENSSL_VERSION}.tar.gz)
set(OPENSSL_HASH SHA256=fa5a4143b8aae18be53ef2f3caf29a2e0747430b8bc74d32d88335b94ab63072
    CACHE STRING "openssl source hash")

set(SODIUM_VERSION 1.0.21 CACHE STRING "libsodium version")
set(SODIUM_MIRROR ${LOCAL_MIRROR}
  https://download.libsodium.org/libsodium/releases
  https://github.com/jedisct1/libsodium/releases/download/${SODIUM_VERSION}-RELEASE
  CACHE STRING "libsodium mirror(s)")
set(SODIUM_SOURCE libsodium-${SODIUM_VERSION}.tar.gz)
set(SODIUM_HASH SHA512=ee8cc2f3f5707b172bf75d8c04afbd5f0c83c6f94dbab3f988f07aab716d96f1662556a59e09b3d83c3bd5c22f59327ad95937bf499d523c86146f4df830f777
  CACHE STRING "libsodium source hash")

set(LIBUV_VERSION 1.52.1 CACHE STRING "libuv version")
set(LIBUV_MIRROR ${LOCAL_MIRROR} https://dist.libuv.org/dist/v${LIBUV_VERSION}
    CACHE STRING "libuv mirror(s)")
set(LIBUV_SOURCE libuv-v${LIBUV_VERSION}.tar.gz)
set(LIBUV_HASH SHA512=3dbb61067c03d025fd880afa2015e67d9cd9de2672df2bfb9901a48759f7bd1285a600b4ac4177c1ee0bf8418c7b967fef356f81e37765e33a818df0aabdc1aa
    CACHE STRING "libuv source hash")

set(ZLIB_VERSION 1.3.2 CACHE STRING "zlib version")
set(ZLIB_MIRROR ${LOCAL_MIRROR} https://github.com/madler/zlib/releases/download/v${ZLIB_VERSION}
    CACHE STRING "zlib mirror(s)")
set(ZLIB_SOURCE zlib-${ZLIB_VERSION}.tar.xz)
set(ZLIB_HASH SHA256=d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3
  CACHE STRING "zlib source hash")

set(SCTPLIB_VERSION 1.0.35 CACHE STRING "sctplib version")
set(SCTPLIB_MIRROR ${LOCAL_MIRROR} https://github.com/dreibh/sctplib/archive/refs/tags
  CACHE STRING "sctplib mirror(s)")
set(SCTPLIB_SOURCE sctplib-${SCTPLIB_VERSION}.tar.gz)
set(SCTPLIB_HASH SHA512=80c521ee4bed0f3442af9a62beec89f3fc15531ba6d46e4b244a49bf412cad0f2dc8274b223dee906da010823a065445d957392ad7553b58ce0dceebe5e5d6c8
  CACHE STRING "sctplib source hash")

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

build_external(sctplib
  CONFIGURE_COMMAND ./autogen.sh && ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared --enable-static --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}"
  BUILD_BYPRODUCTS
    ${DEPS_DESTDIR}/lib/libsctplib.a
    ${DEPS_DESTDIR}/include/netinet/sctp.h
)
add_static_target(sctplib sctplib_external libsctplib.a)


set(openssl_system_env "")
set(openssl_arch "")
set(openssl_configure_command ./config)
set(openssl_flags "CFLAGS=${deps_CFLAGS}")
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
    no-shared no-capieng no-dso no-ec_nistp_64_gcc_128 no-gost
    no-md2 no-rc5 no-rdrand no-rfc3779 no-ssl-trace no-ssl3
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

build_external(sodium CONFIGURE_COMMAND ./configure ${cross_host} ${cross_rc} --prefix=${DEPS_DESTDIR} --disable-shared
          --enable-static --with-pic "CC=${deps_cc}" "CFLAGS=${deps_CFLAGS}")
add_static_target(sodium sodium_external libsodium.a)
