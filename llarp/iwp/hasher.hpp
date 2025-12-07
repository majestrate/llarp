#pragma once
#include <llarp/net/sock_addr.hpp>
#include <llarp/crypto/types.hpp>
#include "message_buffer.hpp"
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/multithreaded_hasher.hpp>
#include <thread>
#include <vector>
namespace llarp::iwp
{
  struct Hasher
  {
    struct VerifyRequest
    {
      InboundMessage msg;
      SockAddr from;
      inline bool
      Verify() const
      {
        return msg.Verify();
      }
    };

    struct VerifyResult
    {
      SockAddr from;
      uint64_t msgid;
      bool result;
      inline bool
      operator<(const VerifyResult& other) const
      {
        return msgid < other.msgid;
      }
    };

    struct HashedMessage
    {
      OutboundMessage msg;
      SockAddr to;
      inline const byte_t*
      data() const
      {
        return msg.data();
      }
      inline size_t
      size() const
      {
        return msg.size();
      }

      inline bool
      operator<(const HashedMessage& other) const
      {
        return msg.m_MsgID < other.msg.m_MsgID;
      }
    };

   private:
    static void
    HashOutboundMessage(const uint8_t* ptr, size_t sz, ShortHash& result);

    static ShortHash&
    GetOutboundMessageHash(HashedMessage& msg);

    llarp::thread::Queue<VerifyRequest> m_VerifyHash{1024};
    llarp::thread::Queue<VerifyResult> m_ProcessVerified{1024};
    std::vector<std::thread> m_Threads;
    util::MultithreadedHasher<HashedMessage, ShortHash> m_Hasher{
        Hasher::HashOutboundMessage, Hasher::GetOutboundMessageHash};
    std::shared_ptr<EventLoopWakeup> m_Waker;

   public:
    void
    start(size_t N_threads, std::shared_ptr<EventLoopWakeup> waker);

    void
    stop();

    bool
    is_running() const;

    std::vector<VerifyResult>
    poll_verified();

    std::vector<HashedMessage>
    poll_hashed();

    void
    async_verify_hash(const InboundMessage& msg, const SockAddr& from);

    void
    async_hash_many(const SockAddr& to, std::vector<OutboundMessage> msgs);
  };

}  // namespace llarp::iwp
