#pragma once
#include <llarp/util/thread/queue.hpp>
#include <llarp/ev/ev.hpp>

#include <memory>
#include <thread>

namespace llarp
{
  struct AbstractRouter;
}

namespace llarp::iwp
{
  static constexpr size_t worker_queue_size = 128;

  struct Session;

  class EncryptWorker
  {
    std::vector<std::jthread> m_Threads;
    void
    Work();

    friend Session;

   public:
    using Packet_t = std::vector<byte_t>;
    EncryptWorker() = default;
    ~EncryptWorker();

    void
    Start(size_t threads);

    void Submit(std::weak_ptr<Session>, Packet_t);

   private:
    thread::Queue<std::pair<std::weak_ptr<Session>, Packet_t>> m_SubmitQueue{worker_queue_size};
    static void
    EncryptPacket(Session*, Packet_t);
    uint64_t m_Seq{};
  };

  class DecryptWorker
  {
    std::vector<std::jthread> m_Threads;

    void
    Work();

   public:
    using Packet_t = std::vector<byte_t>;
    DecryptWorker() = default;
    ~DecryptWorker();

    void
    Start(size_t threads);

    void Submit(std::weak_ptr<Session>, Packet_t);

   private:
    using Event_t = std::pair<uint64_t, Packet_t>;
    thread::Queue<std::pair<std::weak_ptr<Session>, Event_t>> m_SubmitQueue{worker_queue_size};
    uint64_t m_Seq{};
  };

  class Worker
  {
    EncryptWorker m_Encrypt;
    DecryptWorker m_Decrypt;

   public:
    Worker() = default;
    ~Worker() = default;

    void
    Start(size_t threads);

    void Encrypt(std::weak_ptr<Session>, EncryptWorker::Packet_t);

    void Decrypt(std::weak_ptr<Session>, DecryptWorker::Packet_t);
  };

}  // namespace llarp::iwp