#include <llarp/util/alloc.h>
#include "dtls.hpp"
#include "linklayer.hpp"
#include <llarp/util/logging.hpp>
#include <stdexcept>

namespace llarp::dtls
{
  static auto logcat = log::Cat("dtls");

  LinkLayer::LinkLayer(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work,
      bool permitInbound)
      : ILinkLayer{
            keyManager, loop, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, work}
      , m_Inbound{permitInbound}
  {}

  std::shared_ptr<ILinkSession>
  LinkLayer::NewOutboundSession(const RouterContact& rc, const AddressInfo& ai)
  {
    if (m_Inbound)
      throw std::logic_error{"inbound link cannot make outbound sessions"};
    return std::make_shared<Session>(this, rc, ai);
  }

  std::string_view
  LinkLayer::Name() const
  {
    return "dtls";
  }

  uint16_t
  LinkLayer::Rank() const
  {
    return 2;
  }

  void
  LinkLayer::RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt)
  {
    std::shared_ptr<ILinkSession> session;
    auto itr = m_AuthedAddrs.find(from);
    bool isNewSession = false;
    if (itr == m_AuthedAddrs.end())
    {
      Lock_t lock{m_PendingMutex};
      auto it = m_Pending.find(from);
      if (it == m_Pending.end())
      {
        if (not m_Inbound)
          return;
        isNewSession = true;
        it = m_Pending.emplace(from, std::make_shared<Session>(this, from)).first;
      }
      session = it->second;
    }
    else if (auto s_itr = m_AuthedLinks.find(itr->second); s_itr != m_AuthedLinks.end())
      session = s_itr->second;

    if (not session)
    {
      return;
    }

    if (not session->Recv_LL(std::move(pkt)) and isNewSession)
      m_Pending.erase(from);
  }

  std::shared_ptr<Session>
  LinkLayer::SessionForAddr(const SockAddr& addr) const
  {
    if (auto itr = m_Pending.find(addr); itr != m_Pending.end())
      return std::dynamic_pointer_cast<Session>(itr->second);

    if (auto itr = m_AuthedAddrs.find(addr); itr != m_AuthedAddrs.end())
      if (auto sitr = m_AuthedLinks.find(itr->second); sitr != m_AuthedLinks.end())
        return std::dynamic_pointer_cast<Session>(sitr->second);

    return nullptr;
  }

  LinkLayer_ptr
  NewInboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work)
  {
    return std::make_shared<LinkLayer>(
        keyManager,
        loop,
        getrc,
        h,
        sign,
        before,
        est,
        reneg,
        timeout,
        closed,
        pumpDone,
        work,
        true);
  }

  LinkLayer_ptr
  NewOutboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work)
  {
    return std::make_shared<LinkLayer>(
        keyManager,
        loop,
        getrc,
        h,
        sign,
        before,
        est,
        reneg,
        timeout,
        closed,
        pumpDone,
        work,
        false);
  }
}  // namespace llarp::dtls
