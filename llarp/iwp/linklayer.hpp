#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/link/server.hpp>
#include <llarp/config/key_manager.hpp>
#include <llarp/ev/ev.hpp>

#include "hasher.hpp"
#include "session.hpp"

#include <memory>
#include <optional>
#include <unordered_set>

namespace llarp::iwp
{
  struct Session;

  struct LinkLayer final : public ILinkLayer
  {
    LinkLayer(
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
        WorkerFunc_t dowork,
        bool permitInbound);

    ~LinkLayer() override;

    std::shared_ptr<ILinkSession>
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) override;

    std::string_view
    Name() const override;

    uint16_t
    Rank() const override;

    void
    RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt) override;

    void
    WakeupPlaintext();

    std::string
    PrintableName() const;

    Hasher*
    hasher();

    void
    TriggerHashing(std::shared_ptr<Session> s);

    struct SessionAddrHash
    {
      size_t
      hash(const std::shared_ptr<Session>& s) const;

      size_t
      operator()(const std::shared_ptr<Session>& s) const
      {
        return hash(s);
      }
    };

   private:
    void
    HandleWakeupPlaintext();

    const std::shared_ptr<EventLoopWakeup> m_Wakeup, m_HashingWakeup;
    std::vector<ILinkSession*> m_WakingUp;
    std::unordered_set<std::shared_ptr<Session>, SessionAddrHash> m_CollectHash;

    const bool m_Inbound;

    std::shared_ptr<Session>
    SessionForAddr(const SockAddr& addr) const;
  };

  using LinkLayer_ptr = std::shared_ptr<LinkLayer>;
}  // namespace llarp::iwp
