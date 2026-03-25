#pragma once

#include <unordered_map>
#include <llarp/net/net.hpp>
#include <llarp/ev/libuv.hpp>
#include <oxenc/variant.h>

#include <llarp/ev/udp_handle.hpp>

namespace mocks
{
  class Network;

  class MockUDPHandle : public llarp::UDPHandle
  {
    Network* const _net;
    std::optional<llarp::SockAddr> _addr;

   public:
      MockUDPHandle(Network* net, llarp::UDPHandle::ReceiveFunc recv)
        : llarp::UDPHandle{recv}, _net{net}
    {}

    std::optional<llarp::SockAddr>
    LocalAddr() const override
    {
      return _addr;
    }

    bool
    listen(const llarp::SockAddr& addr) override;

    bool
    send(const llarp::SockAddr&, const llarp_buffer_t&) override
    {
      return true;
    };

    void
    close() override{};
  };

  class Network : public llarp::net::Platform, public llarp::EventLoop
  {
    std::unordered_multimap<std::string, llarp::IPRange> _network_interfaces;
    bool _snode;

    const Platform* const m_Default{Platform::Default_ptr()};
    llarp::EventLoop_ptr m_EventLoop;

   public:
    Network(
        std::unordered_multimap<std::string, llarp::IPRange> network_interfaces, bool snode = true)
        : llarp::net::Platform{}
        , _network_interfaces{std::move(network_interfaces)}
        , _snode{snode}
        , m_EventLoop{llarp::EventLoop::create(1, 1024)}
    {}

    ~Network() override = default;

    const llarp::net::Platform*
    Net_ptr() const override
    {
      return this;
    }

    void
    run() override
    {
      m_EventLoop->run();
    };

    size_t
    num_worker_threads() const override
    {
      return m_EventLoop->num_worker_threads();
    }

    void
    queue_slow_work(std::unique_ptr<llarp::EventLoopWork> work) override
    {
      m_EventLoop->queue_slow_work(std::move(work));
    }

    void
    queue_work(std::unique_ptr<llarp::EventLoopWork> work) override
    {
      m_EventLoop->queue_work(std::move(work));
    }

    void wakeup() override
    {
      m_EventLoop->wakeup();
    }

    bool
    inEventLoop() const override
    {
      return m_EventLoop->inEventLoop();
    }

    std::shared_ptr<llarp::EventLoopRepeater>
    make_repeater() override
    {
      return m_EventLoop->make_repeater();
    }

    std::shared_ptr<llarp::EventLoopWakeup>
    make_waker(std::function<void()> callback) override
    {
      return m_EventLoop->make_waker(std::move(callback));
    }

    bool
    add_ticker(std::function<void()> ticker) override
    {
      return m_EventLoop->add_ticker(std::move(ticker));
    }

    bool
    add_network_interface(std::shared_ptr<llarp::vpn::NetworkInterface> netif, std::function<void(llarp::net::IPPacket)> packetHandler) override
    {
      return m_EventLoop->add_network_interface(std::move(netif), std::move(packetHandler));
    }

    std::shared_ptr<llarp::EventLoopPoller>
    add_poller(int fd, std::function<void()> callback) override
    {
      return m_EventLoop->add_poller(fd, std::move(callback));
    }

    void
    call_later(llarp_time_t delay_ms, std::function<void()> callback) override
    {
      m_EventLoop->call_later(delay_ms, std::move(callback));
    }

    void
    call_soon(std::function<void()> f) override
    {
      m_EventLoop->call_soon(std::move(f));
    }

    llarp_time_t
    time_now() const override
    {
      return m_EventLoop->time_now();
    }

    bool
    running() const override
    {
      return m_EventLoop->running();
    }

    void
    stop() override
    {
      m_EventLoop->stop();
    }

    llarp::RuntimeOptions
    Opts() const
    {
      return llarp::RuntimeOptions{false, false, _snode};
    }

    std::shared_ptr<llarp::UDPHandle>
    make_udp(UDPReceiveFunc recv) override
    {
      return std::make_shared<MockUDPHandle>(this, recv);
    }

    std::optional<std::string>
    GetBestNetIF(int af) const override
    {
      for (const auto& [k, range] : _network_interfaces)
        if (range.Family() == af and not IsBogonRange(range))
          return k;
      return std::nullopt;
    }

    std::optional<std::string>
    FindFreeTun() const override
    {
      return "mocktun0";
    }

    std::optional<llarp::SockAddr>
    GetInterfaceAddr(std::string_view ifname, int af) const override
    {
      for (const auto& [name, range] : _network_interfaces)
        if (range.Family() == af and name == ifname)
          return llarp::SockAddr{range.addr};
      return std::nullopt;
    }

    bool
    HasInterfaceAddress(llarp::net::ipaddr_t ip) const override
    {
      for (const auto& item : _network_interfaces)
        if (item.second.Contains(ip))
          return true;
      // check for wildcard
      return IsWildcardAddress(ip);
    }

    std::optional<llarp::SockAddr>
    AllInterfaces(llarp::SockAddr fallback) const override
    {
      return m_Default->AllInterfaces(fallback);
    }

    std::optional<int>
    GetInterfaceIndex(llarp::net::ipaddr_t ip) const override
    {
      return m_Default->GetInterfaceIndex(ip);
    }

    std::optional<llarp::IPRange>
    FindFreeRange() const override
    {
      auto ownsRange = [this](const auto& range) {
        for (const auto& [name, ownRange] : _network_interfaces)
        {
          if (ownRange * range)
            return true;
        }
        return false;
      };
      using namespace llarp;
      // generate possible ranges to in order of attempts
      std::list<IPRange> possibleRanges;
      for (byte_t oct = 16; oct < 32; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(172, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(10, oct, 0, 1, 16));
      }
      for (byte_t oct = 0; oct < 255; ++oct)
      {
        possibleRanges.emplace_back(IPRange::FromIPv4(192, 168, oct, 1, 24));
      }
      // for each possible range pick the first one we don't own
      for (const auto& range : possibleRanges)
      {
        if (not ownsRange(range))
          return range;
      }
      return std::nullopt;
    }

    std::string
    LoopbackInterfaceName() const override
    {
      for (const auto& [name, range] : _network_interfaces)
        if (IsLoopbackAddress(range.addr))
          return name;
      throw std::runtime_error{"no loopback interface?"};
    }

      std::vector<llarp::net::InterfaceInfo>
      AllNetworkInterfaces() const override
      {
          std::map<std::string, llarp::net::InterfaceInfo> _addrs;
          for(const auto & [ifname, range] : _network_interfaces)
          {
             auto & ent = _addrs[ifname];
             ent.name = ifname;
             ent.addrs.emplace_back(range);
          }
          std::vector<llarp::net::InterfaceInfo> infos;
          for(const auto & [name, info] : _addrs)
          {
              infos.emplace_back(info);
              infos.back().index = infos.size();
          }
          return infos;
      }
  };

  bool
  MockUDPHandle::listen(const llarp::SockAddr& addr)
  {
    if (not _net->HasInterfaceAddress(addr.getIP()))
      return false;
    _addr = addr;
    return true;
  }

}  // namespace mocks
