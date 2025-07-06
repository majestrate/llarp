#include <llarp/util/alloc.h>
#include "linklayer.hpp"
#include <llarp/net/sock_addr.hpp>
#include "session.hpp"
#include <cstdint>
#include <llarp/config/key_manager.hpp>
#include <memory>
#include <unordered_set>

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
      : ILinkLayer(
          keyManager, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, worker)
      , m_Wakeup{ev->make_waker([this]() { HandleWakeupPlaintext(); })}
      , m_HashingWakeup{ev->make_waker([this]() {
        for (auto& session : m_CollectHash)
          session->TriggerHashGen();
        m_CollectHash.clear();
      })}
      , m_Inbound{allowInbound}
  {
    m_Hasher.start(
        ev->num_worker_threads(), ev->make_waker([this]() { HandleWorkerCompletion(); }));
  }

  void
  LinkLayer::HandleWorkerCompletion()
  {
    std::unordered_map<SockAddr, std::vector<OutboundMessage>> hashed;
    std::unordered_map<SockAddr, std::vector<uint64_t>> verified;
    std::unordered_map<SockAddr, std::vector<uint64_t>> drop;
    for (const auto& result : m_Hasher.poll_verified())
    {
      if (result.result)
        verified[result.from].emplace_back(result.msgid);
      else
        drop[result.from].emplace_back(result.msgid);
    }
    for (auto& result : m_Hasher.poll_hashed())
    {
      hashed[result.to].emplace_back(std::move(result.msg));
    }
    std::unordered_set<std::shared_ptr<Session>, SessionAddrHash> should_pump;
    for (auto& [addr, msgs] : hashed)
    {
      if (auto session = SessionForAddr(addr))
      {
        session->RecvHashed(std::move(msgs));
        should_pump.emplace(session);
      }
    }
    std::unordered_set<std::shared_ptr<Session>, SessionAddrHash> send_flush;
    for (const auto& [addr, msgids] : verified)
    {
      if (auto session = SessionForAddr(addr))
      {
        send_flush.emplace(session);
        should_pump.emplace(session);
        for (auto msgid : msgids)
          session->VerifiedMessage(msgid);
      }
    }
    for (const auto& session : send_flush)
    {
      session->SendMACK();
    }
    for (const auto& [addr, msgids] : drop)
    {
      if (auto session = SessionForAddr(addr))
      {
        for (auto msgid : msgids)
          session->DropMessage(msgid);
      }
    }
    for (const auto& session : should_pump)
    {
      session->Pump();
    }
  }

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
  {
    m_Hasher.stop();
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
    PumpDone();
  }

}  // namespace llarp::iwp
