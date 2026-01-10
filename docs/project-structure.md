# LLARP project structure

this codebase is a bit large. this is a high level map of the current code structure.

## LLARP executable main functions `(/daemon)`

* `main.cpp`: LLARP daemon executable

## LLARP public headers `(/include)`

`llarp.hpp`: semi-internal C++ header for LLARP executables


## LLARP core library `(/llarp)`

* `/llarp`: contains a few straggling compilation units
* `/llarp/android`: android platform compat shims
* `/llarp/config`: configuration structs, generation/parsing/validating of config files
* `/llarp/consensus`: network consenus and inter relay testing
* `/llarp/constants`: contains all compile time constants
* `/llarp/crypto`: cryptography interface and implementation, includes various secure helpers
* `/llarp/dht`: dht message structs, parsing, validation and handlers of dht related parts of the protocol
* `/llarp/dns`: dns subsytem, dns udp wire parsers, resolver, server, rewriter/interceptor, the works
* `/llarp/ev`: event loop interfaces and implementations
* `/llarp/exit`: `.snode` endpoint "backend"
* `/llarp/handlers`: packet endpoint "frontends"
* `/llarp/iwp`: "internet wire protocol", hacky homegrown durable udp wire protocol used in LLARP
* `/llarp/link`: linklayer (node to node) communcation subsystem
* `/llarp/messages`: linklayer message parsing and handling
* `/llarp/net`: wrappers and helpers for ip addresses / ip ranges / sockaddrs, hides platform specific implemenation details
* `/llarp/path`: onion routing path logic, both client and relay side, path selection algorithms.
* `/llarp/router`: the relm of the god objects
* `/llarp/routing`: routing messages (onion routed messages sent over paths), parsing, validation and handler interfaces.
* `/llarp/service`: `.loki` endpoint "backend"
* `/llarp/tooling`: network simulation tooling
* `/llarp/util`: utility function dumping ground
* `/llarp/vpn`: vpn tunnel implemenation for each supported platform


## component relations

### `/llarp/service` / `/llarp/handlers` / `/llarp/exit`

for all codepaths for traffic over LLARP, there is 2 parts, the "frontend" and the "backend".

the "backend" is responsible for sending and recieving data inside LLARP using our internal formats via paths, it handles flow management, lookups, timeouts, handover, and all state we have inside LLARP.

the "fontend", is a translation layer that takes in IP Packets from the OS, and send it to the backend to go where ever it wants to go, and recieves data from the "backend" and sends it to the OS as an IP Packet.

there are 2 'backends': `.snode` and `.loki`

there are 2 'frontends': "tun" (generic OS vpn interface) and "null" (does nothing)

* `//TODO: the backends need to be split up into multiple sub components as they are a kitchen sink.`
* `//TODO: the frontends blend into the backend too much and need to have their boundery clearer.`


### `/llarp/ev` /  `/llarp/net` / `/llarp/vpn`

these contain most of the os/platform specific bits

* `//TODO: untangle these`


### `/llarp/link` /  `/llarp/iwp`

node to node traffic logic and wire protocol dialects

* `//TODO: make better definitions of interfaces`
* `//TODO: separte implementation details from interfaces`


## platform contrib code `(/contrib)`

grab bag directory for non core related platform specific non source code

* `/contrib/format.sh`: clang-format / jsonnetfmt / swiftformat helper, will check or correct code style.

system layer and packaging related:

* `/contrib/NetworkManager`
* `/contrib/apparmor`
* `/contrib/systemd-resolved`
* `/contrib/llarp-resolvconf`
* `/contrib/bootstrap`

build shims / ci helpers

* `/contrib/ci`
* `/contrib/patches`
* `/contrib/cross`
* `/contrib/android.sh`
* `/contrib/android-configure.sh`
* `/contrib/windows.sh`
* `/contrib/windows-configure.sh`
* `/contrib/mac.sh`
* `/contrib/ios.sh`
* `/contrib/cross.sh`
