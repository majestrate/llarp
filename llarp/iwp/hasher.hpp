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

  struct Session;

  struct Hasher
  {
    struct VerifyRequest
    {
      InboundMessage msg;
      std::weak_ptr<Session> session;

      bool
      Verify() const
      {
        return msg.Verify();
      }
    };

    struct VerifyResult
    {
      std::weak_ptr<Session> session;
      uint64_t msgid;
      bool result;

      bool
      operator<(const VerifyResult& other) const
      {
        return msgid < other.msgid;
      }
    };

    struct HashedMessage
    {
      OutboundMessage msg;
      std::weak_ptr<Session> session;

      const byte_t*
      data() const
      {
        return msg.data();
      }
      size_t
      size() const
      {
        return msg.size();
      }

      bool
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

    thread::Queue<VerifyRequest> m_VerifyHash{128};
    thread::Queue<VerifyResult> m_ProcessVerified{128};
    std::vector<std::thread> m_Threads;
    util::MultithreadedHasher<HashedMessage, ShortHash> m_Hasher{
        HashOutboundMessage, GetOutboundMessageHash};
    std::shared_ptr<EventLoopWakeup> m_Waker;

   public:
    explicit Hasher(EventLoop_ptr ev);

    void
    start(size_t N_threads);

    void
    stop();

    bool
    is_running() const;

    std::vector<VerifyResult>
    poll_verified();

    std::vector<HashedMessage>
    poll_hashed();

    void
    async_verify_hash(const InboundMessage& msg, std::weak_ptr<Session> session);

    void
    async_hash_many(std::weak_ptr<Session> sess, std::vector<OutboundMessage> msgs);
  };

}  // namespace llarp::iwp
