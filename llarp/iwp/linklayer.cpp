#include <llarp/util/alloc.h>
#include "linklayer.hpp"
#include <llarp/net/sock_addr.hpp>
#include "session.hpp"
#include <cstdint>
#include <llarp/config/key_manager.hpp>
#include <memory>
#include <unordered_set>

#include <llarp/router/abstractrouter.hpp>

namespace llarp::iwp
{
  static auto logcat = log::Cat("iwp");

  LinkLayer::LinkLayer(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> ev,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t worker,
      bool allowInbound)
      : ILinkLayer{keyManager, ev, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, worker}
      , m_Wakeup{ev->make_waker([this]() { HandleWakeupPlaintext(); })}
      , m_HashingWakeup{ev->make_waker([this]() {
        for (auto& session : m_CollectHash)
          session->TriggerHashGen();
        m_CollectHash.clear();
      })}
      , m_Inbound{allowInbound}
  {}

  std::shared_ptr<Session>
  LinkLayer::SessionForAddr(const SockAddr& addr) const
  {
    if (auto itr = m_AuthedAddrs.find(addr); itr != m_AuthedAddrs.end())
    {
      if (auto s_itr = m_AuthedLinks.find(itr->second); s_itr != m_AuthedLinks.end())
      {
        return std::dynamic_pointer_cast<Session>(s_itr->second);
      }
    }
    if (auto itr = m_Pending.find(addr); itr != m_Pending.end())
    {
      return std::dynamic_pointer_cast<Session>(itr->second);
    }
    log::error(logcat, "No session for addr: {}", addr);
    return nullptr;
  }

  std::string_view
  LinkLayer::Name() const
  {
    return "iwp";
  }

  std::string
  LinkLayer::PrintableName() const
  {
    if (m_Inbound)
      return "inbound iwp link";
    else
      return "outbound iwp link";
  }

  uint16_t
  LinkLayer::Rank() const
  {
    return 2;
  }

  LinkLayer::~LinkLayer()
  {}

  Hasher*
  LinkLayer::hasher()
  {
    return Router()->linkHasher().get();
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
    else
    {
      if (auto s_itr = m_AuthedLinks.find(itr->second); s_itr != m_AuthedLinks.end())
        session = s_itr->second;
    }
    if (session)
    {
      bool success = session->Recv_LL(std::move(pkt));
      if (not success and isNewSession)
      {
        LogDebug("Brand new session failed; removing from pending sessions list");
        m_Pending.erase(from);
      }
      WakeupPlaintext();
    }
  }

  std::shared_ptr<ILinkSession>
  LinkLayer::NewOutboundSession(const RouterContact& rc, const AddressInfo& ai)
  {
    if (m_Inbound)
      throw std::logic_error{"inbound link cannot make outbound sessions"};
    return std::make_shared<Session>(this, rc, ai);
  }

  void
  LinkLayer::WakeupPlaintext()
  {
    m_Wakeup->Trigger();
    log::debug(logcat, "wakeup plaintext");
  }

  size_t
  LinkLayer::SessionAddrHash::hash(const std::shared_ptr<Session>& s) const
  {
    std::hash<SockAddr> h{};
    return h(s->GetRemoteEndpoint());
  }

  void
  LinkLayer::TriggerHashing(std::shared_ptr<Session> s)
  {
    m_CollectHash.emplace(s);
    m_HashingWakeup->Trigger();
    log::debug(logcat, "trigger hashing");
  }

  void
  LinkLayer::HandleWakeupPlaintext()
  {
    // Copy bare pointers out first because HandlePlaintext can end up removing themselves from
    // the structures.
    m_WakingUp.clear();  // Reused to minimize allocations.
    for (const auto& [router_id, session] : m_AuthedLinks)
      m_WakingUp.push_back(session.get());
    for (const auto& [addr, session] : m_Pending)
      m_WakingUp.push_back(session.get());
    for (auto* session : m_WakingUp)
      session->HandlePlaintext();
  }

}  // namespace llarp::iwp
