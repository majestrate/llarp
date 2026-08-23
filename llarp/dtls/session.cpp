#include <llarp/util/alloc.h>
#include "session.hpp"
#include "linklayer.hpp"

#include <llarp/util/logging.hpp>

#include <openssl/err.h>

#include <array>
#include <utility>

namespace llarp::dtls
{
  static auto logcat = log::Cat("dtls-session");

  static void
  init_openssl_once()
  {
    static const auto initialized = [] {
      SSL_library_init();
      SSL_load_error_strings();
      OpenSSL_add_ssl_algorithms();
      return true;
    }();
    (void)initialized;
  }

  Session::Session(LinkLayer* parent, const RouterContact& rc, const AddressInfo& ai)
      : m_Parent(parent), m_Inbound(false), m_RemoteAddr(ai), m_RemoteRC(rc)
  {
    m_CreatedAt = m_Parent->Now();
    m_LastRX = m_CreatedAt;
    m_LastTX = m_CreatedAt;
    if (not InitTLS())
      throw std::runtime_error{"failed to initialize dtls outbound session"};
  }

  Session::Session(LinkLayer* parent, const SockAddr& from)
      : m_Parent(parent), m_Inbound(true), m_RemoteAddr(from)
  {
    m_CreatedAt = m_Parent->Now();
    m_LastRX = m_CreatedAt;
    m_LastTX = m_CreatedAt;
    if (not InitTLS())
      throw std::runtime_error{"failed to initialize dtls inbound session"};
  }

  Session::~Session()
  {
    if (m_SSL)
      SSL_free(m_SSL);
    if (m_CTX)
      SSL_CTX_free(m_CTX);
  }

  bool
  Session::InitTLS()
  {
    init_openssl_once();

    const SSL_METHOD* method = m_Inbound ? DTLS_server_method() : DTLS_client_method();
    m_CTX = SSL_CTX_new(method);
    if (not m_CTX)
      return false;

    SSL_CTX_set_min_proto_version(m_CTX, DTLS1_2_VERSION);
    SSL_CTX_set_cipher_list(m_CTX, "DEFAULT");

    m_SSL = SSL_new(m_CTX);
    if (not m_SSL)
      return false;

    m_ReadBIO = BIO_new(BIO_s_mem());
    m_WriteBIO = BIO_new(BIO_s_mem());
    if (not m_ReadBIO or not m_WriteBIO)
      return false;

    SSL_set_bio(m_SSL, m_ReadBIO, m_WriteBIO);
    if (m_Inbound)
      SSL_set_accept_state(m_SSL);
    else
      SSL_set_connect_state(m_SSL);

    return true;
  }

  ILinkLayer*
  Session::GetLinkLayer() const
  {
    return m_Parent;
  }

  bool
  Session::FlushCiphertext()
  {
    if (not m_SSL)
      return false;

    std::array<byte_t, 4096> buf{};
    while (BIO_ctrl_pending(m_WriteBIO) > 0)
    {
      const auto n = BIO_read(m_WriteBIO, buf.data(), buf.size());
      if (n <= 0)
        break;
      llarp_buffer_t pkt{buf.data(), static_cast<size_t>(n)};
      m_Parent->SendTo_LL(m_RemoteAddr, pkt);
      m_LastTX = m_Parent->Now();
      m_Stats.totalPacketsRX++;
    }
    return true;
  }

  bool
  Session::PumpHandshake()
  {
    if (m_Closed)
      return false;

    if (m_Established)
      return true;

    const auto rc = SSL_do_handshake(m_SSL);
    if (rc == 1)
    {
      m_Established = true;
      if (not m_Inbound and not m_RemoteRC.pubkey.IsZero())
        m_Parent->MapAddr(m_RemoteRC.pubkey, this);
      m_Parent->SessionEstablished(this, m_Inbound);
      return FlushCiphertext();
    }

    const auto err = SSL_get_error(m_SSL, rc);
    FlushCiphertext();
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
      return true;

    log::warning(logcat, "dtls handshake failed with error {}", err);
    Close();
    return false;
  }

  bool
  Session::DrainApplicationData()
  {
    if (not m_Established or m_Closed)
      return false;

    std::array<byte_t, 4096> plain{};
    while (true)
    {
      const auto n = SSL_read(m_SSL, plain.data(), plain.size());
      if (n > 0)
      {
        Message_t msg(static_cast<size_t>(n));
        std::copy_n(plain.data(), n, msg.data());
        llarp_buffer_t buf{msg};
        (void)m_Parent->HandleMessage(this, buf);
        m_LastRX = m_Parent->Now();
        m_Stats.totalPacketsRX++;
        continue;
      }

      const auto err = SSL_get_error(m_SSL, n);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        return true;

      if (err == SSL_ERROR_ZERO_RETURN)
      {
        Close();
        return false;
      }

      log::warning(logcat, "dtls read failed with error {}", err);
      Close();
      return false;
    }
  }

  bool
  Session::SendAppData(const Message_t& msg, CompletionHandler handler, uint16_t)
  {
    if (m_Closed)
      return false;

    const auto rc = SSL_write(m_SSL, msg.data(), msg.size());
    FlushCiphertext();
    if (rc > 0)
    {
      if (handler)
        handler(DeliveryStatus::eDeliverySuccess);
      m_Stats.totalAckedTX++;
      return true;
    }

    const auto err = SSL_get_error(m_SSL, rc);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
    {
      m_SendQueue.emplace_back(PendingSend{msg, std::move(handler), 0});
      return true;
    }

    if (handler)
      handler(DeliveryStatus::eDeliveryDropped);
    Close();
    return false;
  }

  bool
  Session::SendMessageBuffer(Message_t msg, CompletionHandler handler, uint16_t priority)
  {
    if (m_Closed)
      return false;

    if (not m_Established)
    {
      m_SendQueue.emplace_back(PendingSend{std::move(msg), std::move(handler), priority});
      return true;
    }

    return SendAppData(msg, std::move(handler), priority);
  }

  void
  Session::Start()
  {
    if (m_Started)
      return;
    m_Started = true;
    PumpHandshake();
  }

  void
  Session::Close()
  {
    if (m_Closed)
      return;
    m_Closed = true;
    SSL_shutdown(m_SSL);
  }

  bool
  Session::Recv_LL(Packet_t pkt)
  {
    if (m_Closed)
      return false;

    const auto written = BIO_write(m_ReadBIO, pkt.data(), pkt.size());
    if (written <= 0)
      return false;

    m_LastRX = m_Parent->Now();

    if (not PumpHandshake())
      return false;

    if (not DrainApplicationData())
      return false;

    return FlushCiphertext();
  }

  void
  Session::Pump()
  {
    if (m_Closed)
      return;

    if (not PumpHandshake())
      return;

    if (not m_Established)
      return;

    while (not m_SendQueue.empty())
    {
      auto pending = std::move(m_SendQueue.front());
      m_SendQueue.pop_front();
      if (not SendAppData(pending.msg, std::move(pending.handler), pending.priority))
        break;
    }

    DrainApplicationData();
    FlushCiphertext();
  }

  void
  Session::Tick(llarp_time_t now)
  {
    if (m_Closed)
      return;

    if (ShouldPing())
      SendKeepAlive();

    if (TimedOut(now))
      Close();
  }

  bool
  Session::SendKeepAlive()
  {
    static const Message_t keepalive{0x00};
    return SendMessageBuffer(keepalive, nullptr, 0);
  }

  bool
  Session::IsEstablished() const
  {
    return m_Established and not m_Closed;
  }

  bool
  Session::TimedOut(llarp_time_t now) const
  {
    return (now - m_LastRX) > 30s;
  }

  bool
  Session::RenegotiateSession()
  {
    return false;
  }

  bool
  Session::ShouldPing() const
  {
    return IsEstablished() and (m_Parent->Now() - m_LastTX) > 5s;
  }

  SessionStats
  Session::GetSessionStats() const
  {
    return m_Stats;
  }

  void
  Session::HandlePlaintext()
  {}
}  // namespace llarp::dtls
