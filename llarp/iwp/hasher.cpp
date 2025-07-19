#include <llarp/util/alloc.h>
#include "hasher.hpp"
#include <sodium/crypto_generichash.h>
#include <functional>
#include <llarp/ev/ev.hpp>
#include <llarp/iwp/message_buffer.hpp>
#include <llarp/net/sock_addr.hpp>
#include "llarp/crypto/crypto.hpp"

namespace llarp::iwp
{
  static auto logcat = log::Cat("iwp");

  template <typename InQueue_t, typename OutQueue_t, typename Waker_t>
  static void
  run_worker_thread(InQueue_t* in_q, OutQueue_t* out_q, Waker_t m_Waker)
  {
    llarp::util::SetThreadName("llarpd-iwp-work");
    log::debug(logcat, "iwp work started");
    do
    {
      auto maybe = in_q->popFrontWithTimeout(50ms);
      if (not maybe and not in_q->enabled())
        return;
      if (not maybe)
        continue;
      log::debug(logcat, "got iwp work");
      Hasher::VerifyResult result{std::move(maybe->from), maybe->msg.msgid(), maybe->Verify()};
      log::debug(logcat, "verify {} from {} result={}", result.msgid, result.from, result.result);
      out_q->pushBack(std::move(result));
      m_Waker->Trigger();
    } while (true);
    log::debug(logcat, "iwp work ended");
  }

  void
  Hasher::start(size_t N_workers, std::shared_ptr<EventLoopWakeup> waker)
  {
    if (is_running())
      return;
    log::debug(logcat, "starting hasher with {} threads", N_workers);
    m_Waker = waker;
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
    do
    {
      auto maybe = m_ProcessVerified.tryPopFront();
      if (not maybe)
        break;

      verified.emplace_back(std::move(*maybe));
    } while (true);
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
  Hasher::async_verify_hash(const InboundMessage& msg, const SockAddr& from)
  {
    m_VerifyHash.pushBack(VerifyRequest{msg, from});
  }

  void
  Hasher::async_hash_many(const SockAddr& to, std::vector<OutboundMessage> msgs)
  {
    std::vector<HashedMessage> hashed;
    for (auto& msg : msgs)
      hashed.emplace_back(HashedMessage{std::move(msg), to});
    m_Hasher.async_hash_vec(std::move(hashed));
  }
}  // namespace llarp::iwp
