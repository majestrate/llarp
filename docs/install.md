# Supported Platforms

Tier 1:

* [Linux](#linux-install) (amd64/arm64/mips64el/riscv64)
* [FreeBSD](#freebsd-install)

Tier 2:

* [Android](#apk-install) (maintainer needed)

We will not port to these platforms:

* [Windows](#win32-install)
* [MacOS](#macos-install)
* iPhone
* Homebrew

# Building

Build requirements:

* Git
* CMake
* C++ 17 capable C++ compiler
* libevent >= 2.1
* libsodium >= 1.0.18
* libsystemd

## Linux <span id="linux-install" />

The current most supported platform is Linux. Currently you can compile from source via git.

For Debian, install the following packages:

	$ sudo apt install build-essential cmake git pkg-config libevent-dev libsodium-dev libsystemd-dev

Clone the git repo:

	$ git clone --recursive https://github.com/majestrate/llarp
	$ mkdir llarp/build
	$ cd llarp/build
	$ cmake .. -DCMAKE_BUILD_TYPE=Release
	$ make -j$(nproc)

To install the binary and service files:

	$ sudo make install

## FreeBSD <span id="freebsd-install" />

Currently has no VPN Platform code, this is being fixed shortly.
build:

	$ pkg install cmake git pkgconf sodium libevent
	$ git clone --recursive https://github.com/majestrate/llarp
	$ mkdir llarp/build
	$ cd llarp/build
	$ cmake .. -DCMAKE_BUILD_TYPE=Release
	$ make -j$(nproc)

install (root):

	# make install

## Windows / MacOS / Android <span id="windows-install" />  <span id="macos-install" />

We do not provide official builds for windows or macos as support for these platforms have been discontinued.

The Android APK build is in need of a maintainer.

## Distro Packaging <span id="mom-cancel-my-meetings-arch-linux-broke-again" />

We do not provide any distro specific packaging at this time. If you encountered an issue from that, file an issue with the package maintainer.

# Join the network

Bootstrap into the network, this will prompt the user and guide through this setup.

	$ sudo llarp-opennet-setup --interactive

For automated bootstrap see:

	$ llarp-opennet-setup --help
