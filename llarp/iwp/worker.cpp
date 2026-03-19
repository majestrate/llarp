#include "worker.hpp"
#include "session.hpp"
#include "linklayer.hpp"
#include <llarp/crypto/crypto.hpp>

namespace llarp::iwp
{
  void
  EncryptWorker::Work()
  {
    util::SetThreadName("llarp-encrypt");
    while (m_SubmitQueue.enabled())
    {
      auto maybe = m_SubmitQueue.popFrontWithTimeout(1s);
      if (not maybe)
        continue;
      auto& [weak, pkt] = *maybe;
      auto session = weak.lock();
      if (not session)
        continue;
      EncryptPacket(session.get(), std::move(pkt));
    }
  }
  void
  EncryptWorker::EncryptPacket(Session* session, Packet_t pkt)
  {
    llarp_buffer_t pktbuf{pkt};
    const TunnelNonce nonce_ptr{pkt.data() + HMACSIZE};
    pktbuf.base += PacketOverhead;
    pktbuf.cur = pktbuf.base;
    pktbuf.sz -= PacketOverhead;
    CryptoManager::instance()->xchacha20(pktbuf, session->m_SessionKey, nonce_ptr);
    pktbuf.base = pkt.data() + HMACSIZE;
    pktbuf.sz = pkt.size() - HMACSIZE;
    CryptoManager::instance()->hmac(pkt.data(), pktbuf, session->m_SessionKey);
    session->Send_LL(pkt.data(), pkt.size());
  }

  void
  EncryptWorker::Start(size_t threads)
  {
    if (threads == 0)
      threads = 1;
    while (threads > 0)
    {
      m_Threads.emplace_back([this]() { Work(); });
      --threads;
    }
  }

  void
  EncryptWorker::Submit(std::weak_ptr<Session> ptr, Packet_t pkt)
  {
    m_SubmitQueue.pushBack(std::make_pair(ptr, pkt));
  }

  EncryptWorker::~EncryptWorker()
  {
    m_SubmitQueue.disable();
    for (auto& th : m_Threads)
      th.join();
  }

  void
  DecryptWorker::Work()
  {
    util::SetThreadName("llarp-decrypt");
    while (m_SubmitQueue.enabled())
    {
      auto maybe = m_SubmitQueue.popFrontWithTimeout(1s);
      if (not maybe)
        continue;
      auto& [weak, pkt] = *maybe;
      auto session = weak.lock();
      if (not session)
        continue;

      if (not session->DecryptMessageInPlace(pkt))
      {
        LogError("failed to decrypt session data from ", session->m_RemoteAddr);
        continue;
      }
      if (pkt[PacketOverhead] != llarp::constants::proto_version)
      {
        LogError(
            "protocol version mismatch ",
            int(pkt[PacketOverhead]),
            " != ",
            llarp::constants::proto_version);
        continue;
      }
      LogDebug("decrypted from ", session->m_RemoteAddr);
      session->m_PlaintextRecv.tryPushBack(std::move(pkt));
      session->m_PlaintextEmpty.clear();
      session->m_Parent->WakeupPlaintext();
    }
  }

  void
  DecryptWorker::Start(size_t threads)
  {
    if (threads == 0)
      threads = 1;
    while (threads > 0)
    {
      m_Threads.emplace_back([this]() { Work(); });
      --threads;
    }
  }

  void
  DecryptWorker::Submit(std::weak_ptr<Session> ptr, Packet_t pkt)
  {
    m_SubmitQueue.pushBack(std::make_pair(ptr, pkt));
  }

  DecryptWorker::~DecryptWorker()
  {
    m_SubmitQueue.disable();
    for (auto& th : m_Threads)
      th.join();
  }

  void
  Worker::Encrypt(std::weak_ptr<Session> ptr, EncryptWorker::Packet_t pkt)
  {
    m_Encrypt.Submit(ptr, pkt);
  }
  void
  Worker::Decrypt(std::weak_ptr<Session> ptr, DecryptWorker::Packet_t pkt)
  {
    m_Decrypt.Submit(ptr, pkt);
  }

  void
  Worker::Start(size_t threads)
  {
    m_Encrypt.Start(threads);
    m_Decrypt.Start(threads);
  }

}  // namespace llarp::iwp
