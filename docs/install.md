# Supported Platforms

Tier 1:

* [Linux](#linux-install) (amd64/arm64/mips64el/riscv64)
* [FreeBSD](#freebsd-install)

Tier 2:

* [Android](#apk-install) (maintainer needed)

We will not port to these platforms:

* [Windows](#windows-install)
* [MacOS](#macos-install)
* iPhone
* Homebrew

# Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libuv
* libsodium >= 1.0.18
* libsystemd
* libgeoip (optional)

## Linux <span id="linux-install" />

The current most supported platform is Linux. Currently you can compile from source via git.

For Debian, install the following packages:

    $ sudo apt install build-essential cmake git pkg-config automake libtool libuv1-dev libsodium-dev libsystemd-dev libgeoip-dev

Clone the git repo:

    $ git clone --recursive https://github.com/majestrate/llarp
    $ mkdir llarp/build
    $ cd llarp/build
    $ cmake .. -DBUILD_SHARED_LIBS=OFF
    $ make -j$(nproc)

To install the binary and service files:

    $ sudo make install

An example systemd unit can be found at `contrib/systemd/llarp.service`

## FreeBSD <span id="freebsd-install" />

Currently has no VPN Platform code, this is being fixed shortly.
build:

    $ pkg install cmake git pkgconf sodium
    $ git clone --recursive https://github.com/majestrate/llarp
    $ mkdir llarp/build
    $ cd llarp/build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ make -j$(nproc)

install (root):

    # make install

## Windows / MacOS / Android <span id="windows-install" />  <span id="macos-install" /> <span id="apk-install" />

We do not provide official builds for windows or macos as support for these platforms have been discontinued.

The Android APK build is in need of a maintainer.

## Distro Packaging <span id="mom-cancel-my-meetings-arch-linux-broke-again" />

If you would like to maintain a package for your distro please let us know by opening up a github issue.

We have an [IRC network](ircs://irc.lokinet.io/llarp) if you plan on packaging this software please
idle there as well.

Currently, we do not have any distro packaging.

If you encountered an issue from someone packaging this software,
file an issue with that package maintainer and redirect them to our github issues.