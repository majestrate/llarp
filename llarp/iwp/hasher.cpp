#include <llarp/util/alloc.h>
#include "hasher.hpp"
#include <sodium/crypto_generichash.h>

#include <llarp/ev/ev.hpp>
#include <llarp/iwp/message_buffer.hpp>
#include <llarp/net/sock_addr.hpp>
#include <llarp/crypto/crypto.hpp>
#include <llarp/util/priority_queue.hpp>
#include "session.hpp"
#include "linklayer.hpp"

namespace llarp::iwp
{
  static auto logcat = log::Cat("iwp");

  template <typename InQueue_t, typename OutQueue_t, typename Waker_t>
  static void
  run_worker_thread(InQueue_t* in_q, OutQueue_t* out_q, Waker_t m_Waker)
  {
    util::SetThreadName("llarp-hasher");
    log::debug(logcat, "iwp work started");
    while (in_q->enabled())
    {
      auto maybe = in_q->popFrontWithTimeout(100ms);
      if (not maybe)
        continue;
      log::debug(logcat, "got iwp work");
      Hasher::VerifyResult result{maybe->session, maybe->msg.msgid(), maybe->Verify()};

      out_q->pushBack(std::move(result));
      m_Waker->Trigger();
    }
    log::debug(logcat, "iwp work ended");
  }

  static void
  HandleWorkerCompletion(Hasher&);

  Hasher::Hasher(EventLoop_ptr ev)
  {
    m_Waker = ev->make_waker([this]() { HandleWorkerCompletion(*this); });
  }

  void
  Hasher::start(size_t N_workers)
  {
    if (is_running())
      return;
    log::debug(logcat, "starting hasher with {} threads", N_workers);
    m_VerifyHash.enable();
    m_Hasher.start(N_workers, [w = m_Waker]() { w->Trigger(); });
    while (N_workers > 0)
    {
      m_Threads.emplace_back(
          [this]() { run_worker_thread(&m_VerifyHash, &m_ProcessVerified, m_Waker); });
      N_workers--;
      log::debug(logcat, "started thread {}", N_workers);
    }
  }

  void
  Hasher::stop()
  {
    m_VerifyHash.disable();
    for (auto& thread : m_Threads)
      thread.join();
    m_Threads.clear();
    m_ProcessVerified.disable();
    m_Hasher.stop();
  }

  void
  Hasher::HashOutboundMessage(const uint8_t* ptr, size_t sz, ShortHash& result)
  {
    CryptoManager::instance()->shorthash(result, llarp_buffer_t{ptr, sz});
  }

  ShortHash&
  Hasher::GetOutboundMessageHash(HashedMessage& msg)
  {
    return msg.msg.m_Digest;
  }

  std::vector<Hasher::VerifyResult>
  Hasher::poll_verified()
  {
    std::vector<VerifyResult> verified;
    llarp::util::with_inplace_priority_queue<VerifyResult>(verified, [self = this](auto& queue) {
      do
      {
        auto maybe = self->m_ProcessVerified.tryPopFront();
        if (not maybe)
          break;
        queue.emplace(std::move(*maybe));
      } while (true);
    });

    return verified;
  }

  std::vector<Hasher::HashedMessage>
  Hasher::poll_hashed()
  {
    return m_Hasher.poll_hashed_data();
  }

  bool
  Hasher::is_running() const
  {
    return not m_Threads.empty();
  }

  void
  Hasher::async_verify_hash(const InboundMessage& msg, std::weak_ptr<Session> session)
  {
    m_VerifyHash.pushBack(VerifyRequest{msg, session});
  }

  void
  Hasher::async_hash_many(std::weak_ptr<Session> session, std::vector<OutboundMessage> msgs)
  {
    std::vector<HashedMessage> hashed;
    for (auto& msg : msgs)
      hashed.emplace_back(HashedMessage{std::move(msg), session});
    m_Hasher.async_hash_vec(std::move(hashed));
  }

  static void
  HandleWorkerCompletion(Hasher& hasher)
  {
    log::debug(logcat, "handle worker completion");
    std::unordered_map<std::shared_ptr<Session>, std::vector<OutboundMessage>> hashed;
    std::unordered_map<std::shared_ptr<Session>, std::vector<uint64_t>> verified;
    std::unordered_map<std::shared_ptr<Session>, std::vector<uint64_t>> drop;
    for (const auto& result : hasher.poll_verified())
    {
      auto ptr = result.session.lock();
      if (not ptr)
        continue;
      if (result.result)
        verified[ptr].emplace_back(result.msgid);
      else
        drop[ptr].emplace_back(result.msgid);
    }
    for (auto& result : hasher.poll_hashed())
    {
      if (auto ptr = result.session.lock())
        hashed[ptr].emplace_back(std::move(result.msg));
    }
    std::unordered_set<std::shared_ptr<Session>, LinkLayer::SessionAddrHash> should_pump;
    for (auto& [session, msgs] : hashed)
    {
      session->RecvHashed(std::move(msgs));
      should_pump.emplace(session);
    }

    std::unordered_set<std::shared_ptr<Session>, LinkLayer::SessionAddrHash> send_flush;
    for (const auto& [session, msgids] : verified)
    {
      send_flush.emplace(session);
      should_pump.emplace(session);
      for (auto msgid : msgids)
        session->VerifiedMessage(msgid);
    }
    for (const auto& session : send_flush)
    {
      session->SendMACK();
    }
    for (const auto& [session, msgids] : drop)
    {
      for (auto msgid : msgids)
        session->DropMessage(msgid);
    }
    for (const auto& session : should_pump)
    {
      session->Pump();
    }
  }
}  // namespace llarp::iwp
