#pragma once
#include "ev.hpp"
#include <llarp/util/thread/queue.hpp>
#include <liburing.h>
namespace llarp::io_uring
{

  class Resource;

  class Loop : public llarp::EventLoop, public std::enable_shared_from_this<Loop>
  {
    ::io_uring m_Ring;
    std::optional<std::thread::id> m_EventLoopThreadID;
    using AtomicQueue_t = llarp::thread::Queue<std::function<void(void)>>;
    AtomicQueue_t m_LogicCalls;
    AtomicQueue_t m_DiskCalls;
    AtomicQueue_t m_WorkCalls;
    using Thread_ptr = std::shared_ptr<std::thread>;
    Thread_ptr m_DiskThread;
    std::vector<Thread_ptr> m_WorkerThreads;
    std::vector<std::shared_ptr<Resource>> m_Handles;
    std::vector<std::shared_ptr<Resource>> m_Timers;
    llarp_time_t m_Now;
    std::shared_ptr<EventLoopWakeup> m_LogicWaker;
    std::shared_ptr<EventLoopWakeup> m_TickWaker;
    std::vector<std::function<void()>> m_Tickers;
    std::atomic<bool> m_Run;

    void
    flush_logic();

   public:
    constexpr ::io_uring*
    ring()
    {
      return &m_Ring;
    };
    Loop(size_t queue_size, size_t worker_threads);
    ~Loop() override = default;

    template <typename Visit>
    void
    remove_handles_where(Visit v)
    {
      std::remove_if(m_Handles.begin(), m_Handles.end(), v);
    }

    void
    run() override;

    bool
    running() const override;

    llarp_time_t
    time_now() const override;

    void
    call_later(llarp_time_t delay_ms, std::function<void(void)> callback) override;

    void
    stop() override;

    bool
    add_ticker(std::function<void(void)> ticker) override;

    std::shared_ptr<EventLoopPoller>
    add_poller(int fd, std::function<void(void)> callback) override;

    bool
    add_network_interface(
        std::shared_ptr<llarp::vpn::NetworkInterface> netif,
        std::function<void(llarp::net::IPPacket)> handler) override;

    void
    call_soon(std::function<void(void)> f) override;

    std::shared_ptr<llarp::EventLoopWakeup>
    make_waker(std::function<void()> callback) override;

    std::shared_ptr<EventLoopRepeater>
    make_repeater() override;

    std::shared_ptr<llarp::UDPHandle>
    make_udp(UDPReceiveFunc on_recv) override;

    std::shared_ptr<uvw::Loop>
    MaybeGetUVWLoop() override
    {
      return nullptr;
    }

    void
    wakeup() override;

    bool
    inEventLoop() const override;

    void
    queue_work(std::unique_ptr<EventLoopWork> work) override;

    void
    queue_slow_work(std::unique_ptr<EventLoopWork> work) override;
  };

}  // namespace llarp::io_uring
