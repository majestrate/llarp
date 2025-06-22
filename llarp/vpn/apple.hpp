#pragma once

#include "platform.hpp"
#include "common.hpp"
#include <llarp/vpn/common.hpp>
#include <llarp/util/fd.hpp>
#include <llarp/util/non_blocking.hpp>

#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/kern_event.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
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
#include <variant>

namespace llarp::vpn
{
  class AppleInterface : public NetworkInterface
  {
    std::unique_ptr<util::FD> m_FD;

    static int
    Exec(const std::string& cmd)
    {
      return system(cmd.c_str());
    }

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
      addr.sc_unit = 0;

      if (::connect(m_FD->fd(), (sockaddr*)&addr, sizeof(addr)) < 0)
      {
        m_FD.reset();
        throw std::runtime_error{
            "cannot connect to control socket address: " + std::string{strerror(errno)}};
      }
      uint32_t namesz = IFNAMSIZ;
      std::array<char, IFNAMSIZ + 1> name{};
      if (::getsockopt(m_FD->fd(), SYSPROTO_CONTROL, 2, name.data(), &namesz) < 0)
      {
        m_FD.reset();
        throw std::runtime_error{
            "cannot query for interface name: " + std::string{strerror(errno)}};
      }

      // make underlying file descriptor non blocking
      llarp::util::NonBlocking{*m_FD};

      auto& m_IfName = m_Info.ifname;
      m_IfName = name.data();
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          const huint32_t addr = net::TruncateV6(ifaddr.range.addr);
          const huint32_t netmask = net::TruncateV6(ifaddr.range.netmask_bits);
          const huint32_t daddr = addr & netmask;
          Exec(
              "/sbin/ifconfig " + m_IfName + " " + addr.ToString() + " " + daddr.ToString()
              + " mtu 1500 netmask 255.255.255.255 up");
          Exec(
              "/sbin/route add " + daddr.ToString() + " -netmask " + netmask.ToString()
              + " -interface " + m_IfName);
          Exec("/sbin/route add " + addr.ToString() + " -interface lo0");
        }
        else if (ifaddr.fam == AF_INET6)
        {
          Exec("/sbin/ifconfig " + m_IfName + " inet6 " + ifaddr.range.ToString());
        }
      }
    }

    ~AppleInterface() override = default;

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

      // Prepare storage for header + max-size packet.
      unsigned int pktinfo = 0;
      std::array<iovec, 2> vecs = {iovec{&pktinfo, uintsize}, iovec{pkt.data(), pkt.size()}};
      int sz = ::readv(m_FD->fd(), vecs.data(), vecs.size());
      if (sz >= uintsize)
      {
        pkt.truncate(sz - uintsize);  // shrink to actual size
      }
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        pkt.truncate(0);
        errno = 0;
      }
      else
      {
        throw std::error_code{errno, std::system_category()};
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
   public:
    AppleRouteManager() = default;
    ~AppleRouteManager() override = default;

    // Add a route to a specific IP via a gateway
    void
    AddRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      std::string cmd =
          "/sbin/route add -host " + llarp::net::ToString(ip) + " " + llarp::net::ToString(gateway);
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("AddRoute failed: " + cmd);
    }

    void
    DelRoute(net::ipaddr_t ip, net::ipaddr_t gateway) override
    {
      std::string cmd = "/sbin/route delete -host " + llarp::net::ToString(ip) + " "
          + llarp::net::ToString(gateway);
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("DelRoute failed: " + cmd);
    }

    // Add a default route via the VPN interface's first IPv4 address
    void
    AddDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      const auto& info = vpn.Info();
      if (info.addrs.empty())
        throw std::runtime_error("No interface addresses found");

      std::string gateway = info.addrs[0].range.addr.ToString();
      std::string cmd = "/sbin/route add default " + gateway;
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("AddDefaultRouteViaInterface failed: " + cmd);
    }

    void
    DelDefaultRouteViaInterface(NetworkInterface& vpn) override
    {
      const auto& info = vpn.Info();
      if (info.addrs.empty())
        throw std::runtime_error("No interface addresses found");

      std::string gateway = info.addrs[0].range.addr.ToString();
      std::string cmd = "/sbin/route delete default " + gateway;
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("DelDefaultRouteViaInterface failed: " + cmd);
    }

    // Add a route for a subnet via the VPN interface
    void
    AddRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      std::string cmd = "/sbin/route add -net " + range.addr.ToString() + " -netmask "
          + range.NetmaskString() + " -interface " + vpn.Info().ifname;
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("AddRouteViaInterface failed: " + cmd);
    }

    void
    DelRouteViaInterface(NetworkInterface& vpn, IPRange range) override
    {
      std::string cmd = "/sbin/route delete -net " + range.addr.ToString() + " -netmask "
          + range.NetmaskString() + " -interface " + vpn.Info().ifname;
      int ret = std::system(cmd.c_str());
      if (ret != 0)
        throw std::runtime_error("DelRouteViaInterface failed: " + cmd);
    }

    std::vector<net::ipaddr_t>
    GetGatewaysNotOnInterface(NetworkInterface&) override
    {
      return {};
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
