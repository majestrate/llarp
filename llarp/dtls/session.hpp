#pragma once

#include <llarp/link/session.hpp>

#include <openssl/ssl.h>

#include <deque>

namespace llarp::dtls
{
  struct LinkLayer;

  struct BIO_Deleter
  {
    void
    operator()(BIO* b) const
    {
      Delete(b);
    }
    static void
    Delete(BIO* b);
  };

  struct SSL_CTX_Deleter
  {
    void
    operator()(SSL_CTX* c) const
    {
      Delete(c);
    }
    static void
    Delete(SSL_CTX* c);
  };

  struct SSL_Deleter
  {
    void
    operator()(SSL* s) const
    {
      Delete(s);
    }
    static void
    Delete(SSL* s);
  };

  using BIO_ptr = std::unique_ptr<BIO, BIO_Deleter>;
  using SSL_CTX_ptr = std::unique_ptr<SSL_CTX, SSL_CTX_Deleter>;
  using SSL_ptr = std::unique_ptr<SSL, SSL_Deleter>;

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

    SSL_CTX_ptr m_CTX = nullptr;
    SSL_ptr m_SSL = nullptr;
    BIO_ptr m_ReadBIO = nullptr;
    BIO_ptr m_WriteBIO = nullptr;

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
