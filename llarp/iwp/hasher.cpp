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
  void
  Hasher::run_worker_thread()
  {
    llarp::util::SetThreadName("llarpd-iwp-work");
    do
    {
      auto maybe = m_VerifyHash.popFrontWithTimeout(50ms);
      if (not maybe and not m_VerifyHash.enabled())
        return;
      if (not maybe)
        continue;

      auto result = VerifyResult{std::move(maybe->from), maybe->msg.msgid(), maybe->Verify()};
      m_ProcessVerified.pushBack(std::move(result));
      m_Waker->Trigger();
    } while (true);
  }

  void
  Hasher::start(size_t N_workers, std::shared_ptr<EventLoopWakeup> waker)
  {
    if (is_running())
      return;
    m_Hasher.start(N_workers, [waker]() { waker->Trigger(); });
    while (N_workers > 0)
    {
      m_Threads.emplace_back([this]() { run_worker_thread(); });
      N_workers--;
    }
    m_Waker = waker;
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
