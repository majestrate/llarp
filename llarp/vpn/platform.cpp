
#include "platform.hpp"
#include <cstdint>
#include <stdexcept>
#include <queue>

#ifdef __linux__
#ifdef ANDROID
#include "android.hpp"
#else
#include "linux.hpp"
#endif
#endif
#ifdef __APPLE__
#include "apple.hpp"
#endif

#include <exception>

namespace llarp::vpn
{
  const llarp::net::Platform*
  IRouteManager::Net_ptr() const
  {
    return llarp::net::Platform::Default_ptr();
  }

  class DummyInterface : public NetworkInterface
  {
    std::queue<net::IPPacket> m_GottenPackets;
    std::array<int, 2> m_PipeFD;

   public:
    DummyInterface(InterfaceInfo info) : NetworkInterface{std::move(info)}
    {}

    void
    Start() override
    {
      ::pipe(m_PipeFD.data());
    }

    void
    Stop() override
    {
      ::close(m_PipeFD[1]);
      ::close(m_PipeFD[0]);
    }

    int
    PollFD() const override
    {
      return m_PipeFD[0];
    }

    bool
    WritePacket(net::IPPacket _pkt) override
    {
      // drop invalid packets.
      if (not(_pkt.IsV4() or _pkt.IsV6()))
        return false;
      // don't handle anything except icmp.
      if (_pkt.IsV4() and _pkt.protocol() != static_cast<uint8_t>(net::IPProtocol::ICMP))
        return true;
      if (_pkt.IsV6() and _pkt.protocol() != static_cast<uint8_t>(net::IPProtocol::ICMP6))
        return true;

      // don't handle anything except ping requests.
      if (_pkt.IsV4() and _pkt.icmp_type() != 0x08)
        return true;
      if (_pkt.IsV6() and _pkt.icmp_type() != 128)
        return true;

      bool on_link{false};

      // check if we have the destination address on link
      for (const auto& addr : m_Info.addrs)
      {
        // for now we check if it's on the range.
        if (addr.range.Family() == _pkt.AF() and addr.range.Contains(_pkt.dstaddr()))
          on_link = true;
      }
      // dont reply to packets sent to offlink destinations.
      if (not on_link)
        return true;
      // reply to icmp ping.
      auto pkt = net::IPPacket::make_icmp_reply(_pkt);

      byte_t x{};
      ::write(m_PipeFD[1], &x, sizeof(x));
      m_GottenPackets.emplace(std::move(pkt));

      return true;
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt{};
      if (not m_GottenPackets.empty())
      {
        byte_t x{};
        ::read(m_PipeFD[0], &x, sizeof(x));
        pkt = std::move(m_GottenPackets.front());
        m_GottenPackets.pop();
      }
      return pkt;
    }
  };

  std::shared_ptr<NetworkInterface>
  Platform::CreateDummyInterface(InterfaceInfo info)
  {
    auto netif = std::make_shared<DummyInterface>(std::move(info));
    netif->Start();
    return std::static_pointer_cast<NetworkInterface>(netif);
  }

  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::shared_ptr<Platform> plat;
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_shared<vpn::AndroidPlatform>(ctx);
#else
    plat = std::make_shared<vpn::LinuxPlatform>();
#endif
#endif
#ifdef __APPLE__
    plat = std::make_shared<vpn::ApplePlatform>();
#endif
    if (not plat)
      throw std::runtime_error{"no vpn platform supported on your platform"};
    return plat;
  }

}  // namespace llarp::vpn
