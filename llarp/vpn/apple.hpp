#pragma once

#include "platform.hpp"
#include "common.hpp"
#include <llarp/vpn/common.hpp>
#include <llarp/util/bits.hpp>
#include <llarp/util/fd.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/util/non_blocking.hpp>

#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/kern_event.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>

#include <fcntl.h>
#include <ifaddrs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include <variant>
#include <vector>

namespace llarp::vpn
{
  // The lo0 host route for the tun address must be removed when the daemon
  // exits, but the AppleInterface object is retained by event-loop handles
  // beyond context teardown, so its destructor does not reliably run before
  // process exit.  Track added routes and flush them via atexit(3); the
  // handler must not log, as logging may already be torn down by then.
  namespace apple_route_cleanup
  {
    inline std::vector<std::string>&
    Routes()
    {
      // Deliberately heap-allocated and never freed: atexit handlers and
      // static destructors run interleaved in reverse registration order, so
      // a plain static vector could be destroyed before Flush() runs and it
      // would then iterate a dead object.  A leaked vector has no destructor
      // to register, making Flush() safe no matter when it fires.
      static auto* routes = new std::vector<std::string>();
      return *routes;
    }

    inline void
    Flush()
    {
      for (const auto& ip : Routes())
        ::system(("/sbin/route -n delete -host " + ip + " -interface lo0").c_str());
      Routes().clear();
    }

    inline void
    Track(std::string ip)
    {
      static bool registered = (::atexit(&Flush), true);
      (void)registered;
      Routes().push_back(std::move(ip));
    }
  }  // namespace apple_route_cleanup

  // UTUN_OPT_IFNAME from <net/if_utun.h>, hardcoded so we don't depend on that
  // header existing in older SDKs.
  inline constexpr int apple_utun_opt_ifname = 2;

  class AppleInterface : public NetworkInterface
  {
    std::unique_ptr<util::FD> m_FD;

    static void
    Exec(const std::string& cmd, bool must_succeed = true)
    {
      static auto logcat = log::Cat("vpn.apple");
      log::info(logcat, "exec: {}", cmd);
      if (int ret = ::system(cmd.c_str()); ret != 0 and must_succeed)
        throw std::runtime_error{"command failed (" + std::to_string(ret) + "): " + cmd};
    }

    friend class AppleRouteManager;

   public:
    AppleInterface(InterfaceInfo info)
        : NetworkInterface{std::move(info)}
        , m_FD{std::make_unique<util::FD>(::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL))}
    {
      if (m_FD->fd() == -1)
        throw std::invalid_argument{"cannot open control socket: " + std::string{strerror(errno)}};

      ctl_info cinfo{};
      const std::string apple_utun = "com.apple.net.utun_control";
      std::copy_n(apple_utun.c_str(), apple_utun.size(), cinfo.ctl_name);

      {
        vpn::IOCTL ioc{*m_FD};
        ioc.ioctl(CTLIOCGINFO, &cinfo);
      }
      sockaddr_ctl addr{};
      addr.sc_id = cinfo.ctl_id;

      addr.sc_len = sizeof(addr);
      addr.sc_family = AF_SYSTEM;
      addr.ss_sysaddr = AF_SYS_CONTROL;
      addr.sc_unit = 0;  // kernel picks the first free utunN

      if (::connect(m_FD->fd(), (sockaddr*)&addr, sizeof(addr)) < 0)
      {
        m_FD.reset();
        throw std::runtime_error{
            "cannot connect to control socket address: " + std::string{strerror(errno)}};
      }
      uint32_t namesz = IFNAMSIZ;
      std::array<char, IFNAMSIZ + 1> name{};
      if (::getsockopt(m_FD->fd(), SYSPROTO_CONTROL, apple_utun_opt_ifname, name.data(), &namesz)
          < 0)
      {
        m_FD.reset();
        throw std::runtime_error{
            "cannot query for interface name: " + std::string{strerror(errno)}};
      }

      // make underlying file descriptor non blocking
      llarp::util::NonBlocking{*m_FD};

      auto& m_IfName = m_Info.ifname;
      m_IfName = name.data();
      m_Info.index = if_nametoindex(m_IfName.c_str());
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          const huint32_t addr = net::ToHost(net::TruncateV6(ifaddr.range.addr));
          const huint32_t netmask = net::ToHost(net::TruncateV6(ifaddr.range.netmask_bits));
          const huint32_t daddr = addr & netmask;
          // utun is point-to-point only; use the OpenVPN-style "topology subnet"
          // trick: /32 p2p pair to the network base address, then route the whole
          // range at the interface.
          Exec(
              "/sbin/ifconfig " + m_IfName + " " + addr.ToString() + " " + daddr.ToString()
              + " mtu " + std::to_string(m_Info.mtu) + " netmask 255.255.255.255 up");
          Exec(
              "/sbin/route -n add -net " + daddr.ToString() + " -netmask " + netmask.ToString()
              + " -interface " + m_IfName);
          // our own tun address is local, macOS wants it via loopback:
          Exec("/sbin/route -n add -host " + addr.ToString() + " -interface lo0");
          apple_route_cleanup::Track(addr.ToString());
        }
        else if (ifaddr.fam == AF_INET6)
        {
          const auto prefixlen = bits::count_bits(ifaddr.range.netmask_bits);
          Exec(
              "/sbin/ifconfig " + m_IfName + " inet6 " + ifaddr.range.addr.ToString()
              + " prefixlen " + std::to_string(prefixlen) + " up");
          Exec(
              "/sbin/route -n add -inet6 -net " + ifaddr.range.addr.ToString() + " -prefixlen "
                  + std::to_string(prefixlen) + " -interface " + m_IfName,
              false);
        }
      }
    }

    ~AppleInterface() override
    {
      // Normally unreachable before process exit (see apple_route_cleanup);
      // when it does run, clean up here and de-register so the atexit flush
      // does not delete the same routes twice.
      auto& routes = apple_route_cleanup::Routes();
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          const auto ip = net::TruncateV6(ifaddr.range.addr).ToString();
          Exec("/sbin/route -n delete -host " + ip + " -interface lo0", false);
          routes.erase(std::remove(routes.begin(), routes.end(), ip), routes.end());
        }
      }
    }

    int
    PollFD() const override
    {
      return m_FD->fd();
    }

    net::IPPacket
    ReadNextPacket() override
    {
      constexpr int uintsize = sizeof(unsigned int);
      net::IPPacket pkt{net::IPPacket::MaxSize};

      // Each utun datagram is prefixed with a 4-byte address family header.
      unsigned int pktinfo = 0;
      std::array<iovec, 2> vecs = {iovec{&pktinfo, uintsize}, iovec{pkt.data(), pkt.size()}};
      auto sz = ::readv(m_FD->fd(), vecs.data(), vecs.size());
      if (sz >= uintsize)
      {
        pkt.truncate(sz - uintsize);  // shrink to actual size
      }
      else if (sz < 0)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          pkt.truncate(0);
          errno = 0;
        }
        else
          throw std::error_code{errno, std::system_category()};
      }
      else  // 0 <= sz < uintsize: short read of the AF header; drop it
      {
        pkt.truncate(0);
      }
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      static unsigned int af4 = htonl(AF_INET);
      static unsigned int af6 = htonl(AF_INET6);
      const void* af_ptr =
          pkt.IsV6() ? static_cast<const void*>(&af6) : static_cast<const void*>(&af4);
      size_t af_len = sizeof(unsigned int);

      std::array<iovec, 2> vecs = {
          iovec{const_cast<void*>(af_ptr), af_len},
          iovec{const_cast<byte_t*>(pkt.data()), pkt.size()}};
      ssize_t n = ::writev(m_FD->fd(), vecs.data(), vecs.size());
      if (n >= static_cast<ssize_t>(af_len))
      {
        n -= af_len;
        return static_cast<size_t>(n) == pkt.size();
      }
      return false;
    }
  };

  class AppleRouteManager : public IRouteManager
  {
    static void
    Exec(const std::string& cmd, bool must_succeed = true)
    {
      AppleInterface::Exec(cmd, must_succeed);
    }

    static std::string
    FamilyFlag(const net::ipaddr_t& ip)
    {
      if (std::holds_alternative<net::ipv6addr_t>(ip))
        return "-inet6 ";
      return "";
    }

   public:
    AppleRouteManager() = default;
    ~AppleRouteManager() override = default;

    // Add a first-hop hole-poke route to a specific IP via a physical gateway
    void
    AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      Exec(
          "/sbin/route -n add " + FamilyFlag(ip) + "-host " + llarp::net::ToString(ip) + " "
          + llarp::net::ToString(gateway));
    }

    void
    DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      Exec(
          "/sbin/route -n delete " + FamilyFlag(ip) + "-host " + llarp::net::ToString(ip) + " "
              + llarp::net::ToString(gateway),
          false);
    }

    // Capture all traffic at the tun interface WITHOUT touching the existing
    // default route: two /1 routes (plus four /2s for IPv6) are more specific
    // than "default", so the original gateway stays in the table for the
    // hole-poked first-hop routes added above.
    void
    AddDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      const auto& ifname = vpn.Info().ifname;
      for (const auto* range : {"0.0.0.0/1", "128.0.0.0/1"})
        Exec(std::string{"/sbin/route -n add -net "} + range + " -interface " + ifname);

      bool have_v6 = false;
      for (const auto& addr : vpn.Info().addrs)
        have_v6 |= (addr.fam == AF_INET6);
      if (have_v6)
        for (const auto* base : {"::", "4000::", "8000::", "c000::"})
          Exec(
              std::string{"/sbin/route -n add -inet6 -net "} + base + " -prefixlen 2 -interface "
                  + ifname,
              false);
    }

    void
    DelDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      const auto& ifname = vpn.Info().ifname;
      for (const auto* range : {"0.0.0.0/1", "128.0.0.0/1"})
        Exec(std::string{"/sbin/route -n delete -net "} + range + " -interface " + ifname, false);

      for (const auto* base : {"::", "4000::", "8000::", "c000::"})
        Exec(
            std::string{"/sbin/route -n delete -inet6 -net "} + base + " -prefixlen 2 -interface "
                + ifname,
            false);
    }

    // Add a route for a subnet via the VPN interface
    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      const auto& ifname = vpn.Info().ifname;
      if (range.IsV4())
        Exec(
            "/sbin/route -n add -net " + net::TruncateV6(range.addr).ToString() + " -netmask "
            + range.NetmaskString() + " -interface " + ifname);
      else
        Exec(
            "/sbin/route -n add -inet6 -net " + range.addr.ToString() + " -prefixlen "
            + std::to_string(bits::count_bits(range.netmask_bits)) + " -interface " + ifname);
    }

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      const auto& ifname = vpn.Info().ifname;
      if (range.IsV4())
        Exec(
            "/sbin/route -n delete -net " + net::TruncateV6(range.addr).ToString() + " -netmask "
                + range.NetmaskString() + " -interface " + ifname,
            false);
      else
        Exec(
            "/sbin/route -n delete -inet6 -net " + range.addr.ToString() + " -prefixlen "
                + std::to_string(bits::count_bits(range.netmask_bits)) + " -interface " + ifname,
            false);
    }

    // Enumerate default-route gateways that do NOT live on the tun interface,
    // by dumping the routing table via the classic BSD sysctl interface.  The
    // route poker uses this to find the physical gateway for hole-poking.
    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(NetworkInterface& vpn) override
    {
      std::vector<net::ipaddr_t> gateways;
      const unsigned int tun_index = if_nametoindex(vpn.Info().ifname.c_str());

      int mib[6] = {CTL_NET, PF_ROUTE, 0, 0 /* all families */, NET_RT_FLAGS, RTF_GATEWAY};
      size_t needed = 0;
      if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) != 0)
        return gateways;
      std::vector<char> buf;
      buf.resize(needed);
      if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) != 0)
        return gateways;

      // routing socket sockaddrs are packed with 4-byte alignment; sa_len == 0
      // still occupies one alignment unit
      constexpr auto align = sizeof(uint32_t);
      const auto sa_size = [](const sockaddr* sa) {
        return sa->sa_len ? ((sa->sa_len + align - 1) & ~(align - 1)) : align;
      };

      for (char* ptr = buf.data(); ptr + sizeof(rt_msghdr) <= buf.data() + needed;)
      {
        auto* rtm = reinterpret_cast<rt_msghdr*>(ptr);
        if (rtm->rtm_msglen == 0)
          break;
        char* addrs = ptr + sizeof(rt_msghdr);
        ptr += rtm->rtm_msglen;

        if (rtm->rtm_version != RTM_VERSION)
          continue;
        if ((rtm->rtm_flags & (RTF_UP | RTF_GATEWAY)) != (RTF_UP | RTF_GATEWAY))
          continue;
        if (rtm->rtm_index == tun_index)
          continue;  // route lives on our own interface
        if ((rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY)) != (RTA_DST | RTA_GATEWAY))
          continue;

        const sockaddr* dst = nullptr;
        const sockaddr* gw = nullptr;
        char* sa_ptr = addrs;
        for (int i = 0; i < RTAX_MAX; i++)
        {
          if (not(rtm->rtm_addrs & (1 << i)))
            continue;
          auto* sa = reinterpret_cast<const sockaddr*>(sa_ptr);
          if (i == RTAX_DST)
            dst = sa;
          else if (i == RTAX_GATEWAY)
            gw = sa;
          sa_ptr += sa_size(sa);
        }
        if (not dst or not gw)
          continue;

        // only default routes (dst 0.0.0.0); the poker only consumes IPv4
        if (dst->sa_family != AF_INET or gw->sa_family != AF_INET)
          continue;
        if (reinterpret_cast<const sockaddr_in*>(dst)->sin_addr.s_addr != 0)
          continue;

        gateways.emplace_back(
            net::ipv4addr_t{reinterpret_cast<const sockaddr_in*>(gw)->sin_addr.s_addr});
      }
      return gateways;
    }
  };

  class ApplePlatform : public Platform
  {
    AppleRouteManager _routeManager{};

   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info, AbstractRouter*) override
    {
      return std::static_pointer_cast<NetworkInterface>(
          std::make_shared<AppleInterface>(std::move(info)));
    };

    IRouteManager&
    RouteManager() override
    {
      return _routeManager;
    }
  };
}  // namespace llarp::vpn
