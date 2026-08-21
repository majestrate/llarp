#pragma once

#include <llarp/link/session.hpp>

#include <openssl/ssl.h>

#include <deque>

namespace llarp::dtls
{
  struct LinkLayer;

  struct Session final : ILinkSession, std::enable_shared_from_this<Session>
  {
    Session(LinkLayer* parent, const RouterContact& rc, const AddressInfo& ai);
    Session(LinkLayer* parent, const SockAddr& from);
    ~Session() override;

    void
    Pump() override;

    void
    Tick(llarp_time_t now) override;

    bool
    SendMessageBuffer(Message_t msg, CompletionHandler handler, uint16_t priority) override;

    void
    Start() override;

    void
    Close() override;

    bool
    Recv_LL(Packet_t pkt) override;

    bool
    SendKeepAlive() override;

    bool
    IsEstablished() const override;

    bool
    TimedOut(llarp_time_t now) const override;

    PubKey
    GetPubKey() const override
    {
      return m_RemoteRC.pubkey;
    }

    bool
    IsInbound() const override
    {
      return m_Inbound;
    }

    const SockAddr&
    GetRemoteEndpoint() const override
    {
      return m_RemoteAddr;
    }

    RouterContact
    GetRemoteRC() const override
    {
      return m_RemoteRC;
    }

    size_t
    SendQueueBacklog() const override
    {
      return m_SendQueue.size();
    }

    ILinkLayer*
    GetLinkLayer() const override;

    bool
    RenegotiateSession() override;

    bool
    ShouldPing() const override;

    SessionStats
    GetSessionStats() const override;

    void
    HandlePlaintext() override;

   private:
    struct PendingSend
    {
      Message_t msg;
      CompletionHandler handler;
      uint16_t priority;
    };

    bool
    InitTLS();

    bool
    PumpHandshake();

    bool
    DrainApplicationData();

    bool
    FlushCiphertext();

    bool
    SendAppData(const Message_t& msg, CompletionHandler handler, uint16_t priority);

    LinkLayer* const m_Parent;
    const bool m_Inbound;
    const SockAddr m_RemoteAddr;
    RouterContact m_RemoteRC;

    SSL_CTX* m_CTX = nullptr;
    SSL* m_SSL = nullptr;
    BIO* m_ReadBIO = nullptr;
    BIO* m_WriteBIO = nullptr;

    bool m_Started = false;
    bool m_Established = false;
    bool m_Closed = false;

    llarp_time_t m_CreatedAt = 0s;
    llarp_time_t m_LastRX = 0s;
    llarp_time_t m_LastTX = 0s;

    SessionStats m_Stats;
    std::deque<PendingSend> m_SendQueue;
  };
}  // namespace llarp::dtls
