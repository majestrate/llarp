# What does LLARP actually do?

LLARP is an onion routed authenticated unicast IP network. It exposes an IP tunnel to the user and provides a dns resolver that maps `.loki` and `.snode` gtld onto a user defined ip range.

LLARP allows users to tunnel arbitrary ip ranges to go to a `.loki` address to act as a tunnel broker via another network accessible via another LLARP client. This is commonly known as an "exit node" but the way LLARP does this is much more generic so that term is not very accurate given what it actually does.

The `.snode` gtld refers to a router on the network by its public ed25519 key.

The `.loki` gtld refers to clients that publish the existence anonymously to the network by their ed25519 public key. (`.loki` also has the ability to use short names resolved via external consensus method, like a blockchain).

# How Do I use LLARP?

set system dns resolver to use the dns resolver provided by LLARP, make sure the upstream dns provider that LLARP uses for non LLARP gtlds is set as desired (see llarpd.ini `[dns]` section)

configure exit traffic provider if you want to tunnel ip traffic via LLARP, by default this is off as we cannot provide a sane defualt that makes everyone happy. to enable an exit node, see llarpd.ini `[network]` section, add multiple `exit-node=exitaddrgoeshere.loki` lines for each endpoint you want to use for exit traffic. each `exit-node` entry will be used to randomly stripe across per IP you are sending to. 

note: per flow (ip+proto/port) isolation is trivial on a technical level but currently not implemented at this time.

# Can I run LLARP on a soho router

Yes and that is the best way to run it in practice.

It is quite nice to DIY. IF you choose to do so there is some assembly required:

on the LLARP side, make sure that the...

* ip ranges for `.loki` and `.snode` are statically set (see llarpd.ini `[network]` section `ifaddr=` option)
* network interace used by LLARP is statically set (see llarpd.ini `[network]` section `ifname=` option)
* dns socket is bound to an address the soho router's dns resolver can talk to, see `[dns]` section `bind=` option) 

on the soho router side:

* route queries for `.loki` and `.snode` gtld to go to LLARP dns on soho router's dns resolver
* use dhcp options to set dns to use the soho router's dns resolver
* make sure that the ip ranges for LLARP are reachable via the LAN interface 
* if you are tunneling over an exit ensure that LAN traffic will only forward to go over the LLARP vpn interface
