#pragma once
#ifdef WITH_LIBUV
#include "ev.hpp"
#include "udp_handle.hpp"
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/meta/memfn.hpp>
#include <uvw/loop.h>
#include <uvw/async.h>
#include <uvw/poll.h>

#include <functional>
#include <map>
#include <vector>
#include <unordered_map>

namespace llarp::uv
{
  class UVWakeup;
  class UVRepeater;
  class UDPHandle;

  class TCPConnectionPoolImpl;
  class TCPAcceptorImpl;

  class Loop : public llarp::EventLoop
  {
   public:
    friend UDPHandle;
    friend TCPConnectionPoolImpl;
    friend TCPAcceptorImpl;
    using Callback = std::function<void()>;

    Loop(size_t queue_size, size_t worker_num_threads);
    ~Loop() override;

    virtual void
    run() override;

    bool
    running() const override;

    llarp_time_t
    time_now() const override
    {
      return m_Impl->now();
    }

    void
    call_later(llarp_time_t delay_ms, std::function<void(void)> callback) override;

    void
    tick_event_loop();

    void
    stop() override;

    bool
    add_ticker(std::function<void(void)> ticker) override;

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

    virtual std::shared_ptr<llarp::UDPHandle>
    make_udp(UDPReceiveFunc on_recv) override;

    std::shared_ptr<EventLoopPoller>
    add_poller(int fd, std::function<void()> callback) override;

    void
    FlushLogic();

    std::shared_ptr<uvw::Loop>
    MaybeGetUVWLoop() override;

    bool
    inEventLoop() const override;

    void
    queue_work(std::unique_ptr<EventLoopWork> work) override;
    void
    queue_slow_work(std::unique_ptr<EventLoopWork> work) override;

    size_t
    num_worker_threads() const override;

    void
    add_closer(std::function<void(void)> f);

    TCPConnectionPool&
    connection_pool() override;

   protected:
    std::shared_ptr<uvw::Loop> m_Impl;
    std::optional<std::thread::id> m_EventLoopThreadID;

    void
    io_cycle_complete();

   private:
    std::shared_ptr<uvw::AsyncHandle> m_WakeUp;
    std::atomic<bool> m_Run;
    using AtomicQueue_t = llarp::thread::Queue<std::function<void(void)>>;
    AtomicQueue_t m_LogicCalls;
    AtomicQueue_t m_DiskCalls;
    AtomicQueue_t m_WorkCalls;
    std::unique_ptr<std::thread> m_DiskThread;
    std::vector<std::thread> m_WorkThreads;
    std::vector<std::function<void()>> m_closers, m_tickers;
    std::shared_ptr<TCPConnectionPool> m_ConnectionPool;

#ifdef LOKINET_DEBUG
    uint64_t last_time;
    uint64_t loop_run_count;
#endif
    std::atomic<uint32_t> m_nextID;

    std::map<uint32_t, Callback> m_pendingCalls;

    std::unordered_map<int, std::shared_ptr<uvw::PollHandle>> m_Polls;

    void
    wakeup() override;
  };

}  // namespace llarp::uv
#endif