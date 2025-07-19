#include "io_uring.hpp"
#include <asm-generic/errno.h>
#include <bits/types/struct_iovec.h>
#include <bits/types/struct_itimerspec.h>
#include <liburing.h>
#include <sys/socket.h>
#include <cstdint>
#include <ctime>
#include <sys/timerfd.h>
#include <llarp/util/logging.hpp>
#include <llarp/net/ip_packet.hpp>
#include <llarp/vpn/platform.hpp>
#include <llarp/net/sock_addr.hpp>
#include <llarp/util/buffer.hpp>
#include "llarp/ev/ev.hpp"
#include "llarp/util/types.hpp"
#include "udp_handle.hpp"

#include <memory_resource>
#include <new>
#include <queue>
#include <stdexcept>

namespace llarp::io_uring
{
  auto cat = llarp::log::Cat("io_uring");

  class Submission
  {
    ::io_uring* m_Ring;
    Resource* const m_Res;
    ::io_uring_sqe*
    make_sqe()
    {
      auto* sqe = io_uring_get_sqe(m_Ring);
      io_uring_sqe_set_data(sqe, m_Res);
      return sqe;
    };

   public:
    Submission(Loop& loop, Resource* res) : m_Ring{loop.ring()}, m_Res{res}
    {}

    ~Submission()
    {
      io_uring_submit(m_Ring);
    }

    void
    poll(int fd, short events)
    {
      llarp::log::debug(cat, "poll() fd={} events={}", fd, int{events});
      io_uring_prep_poll_add(make_sqe(), fd, events);
    }

    void
    sendmsg(int fd, const msghdr* msg, unsigned int flags)
    {
      llarp::log::debug(cat, "sendmsg() fd={} flags={}", fd, flags);
      io_uring_prep_sendmsg(make_sqe(), fd, msg, flags);
    }

    void
    recvmsg(int fd, msghdr* msg, unsigned int flags)
    {
      llarp::log::debug(cat, "recvmsg() fd={} flags={}", fd, flags);
      io_uring_prep_recvmsg(make_sqe(), fd, msg, flags);
    }
  };

  class Resource : public std::enable_shared_from_this<Resource>
  {
   protected:
    Loop& m_Loop;

    void
    remove_from_loop()
    {
      m_Loop.remove_handles_where([self = this](const auto& ptr) { return ptr.get() == self; });
    }

   public:
    explicit Resource(Loop& loop) : m_Loop{loop}
    {}

    constexpr ::io_uring*
    ring()
    {
      return m_Loop.ring();
    };

    /// handle event completion.
    virtual void
    completed(int res) = 0;

    /// return if this resource should wake up tickers.
    virtual bool
    should_wake_tickers() const = 0;

    /// submit events to submission queue
    virtual void
    submit() = 0;

    /// shutdown operation
    virtual void
    shutdown() = 0;

    /// returns true if it is safe to remove this resource from the event loop.
    virtual bool
    done() const = 0;

    virtual std::string_view
    name() const = 0;
  };

  class Poller : public EventLoopPoller, public Resource
  {
    int m_FD;
    std::function<void()> m_Callback;
    bool m_Stop{false};

    void
    poll()
    {
      if (m_Stop)
        return;
      Submission sub{m_Loop, this};
      sub.poll(m_FD, POLL_IN);
    }

   public:
    Poller(Loop& loop, int fd, std::function<void()> cb)
        : Resource{loop}, m_FD{fd}, m_Callback{std::move(cb)}
    {}

    std::string_view
    name() const override
    {
      return "poller";
    }

    bool
    done() const override
    {
      return m_Stop;
    }

    void
    submit() override
    {
      poll();
    }

    void
    shutdown() override
    {
      m_Stop = true;
    }

    void
    completed(int res) override
    {
      if (m_Stop)
        return;

      if (res > 0)
        m_Callback();

      if (res < 0)
        shutdown();
      else
        poll();
    }

    bool
    should_wake_tickers() const override
    {
      return not m_Stop;
    }

    void
    close() override
    {
      submit();
      shutdown();
    }
  };

  struct UDPSocket : public llarp::UDPHandle, public Resource
  {
    static constexpr size_t ent_size = 1500;
    static constexpr size_t max_packets = 1024;
    struct buffer_inner
    {
      msghdr hdr{};
      iovec vec{};
      sockaddr_storage addr{};
      explicit buffer_inner(const SockAddr& dst, byte_t* ptr, size_t sz)
      {
        std::copy_n(
            reinterpret_cast<const byte_t*>(dst.operator const sockaddr*()),
            dst.sockaddr_len(),
            reinterpret_cast<byte_t*>(&addr));
        hdr.msg_name = &addr;
        hdr.msg_namelen = dst.sockaddr_len();
        hdr.msg_iov = &vec;
        hdr.msg_iovlen = 1;
        vec.iov_base = ptr;
        vec.iov_len = sz;
      };

      explicit buffer_inner(byte_t* ptr, size_t sz)
      {
        hdr.msg_name = &addr;
        hdr.msg_iov = &vec;
        hdr.msg_iovlen = 1;
        vec.iov_base = ptr;
        vec.iov_len = sz;
      }
    };

#pragma pack(1)
    struct send_buffer_t
    {
      std::array<byte_t, ent_size - sizeof(buffer_inner)> _buf{};
      buffer_inner inner;

      explicit send_buffer_t(const SockAddr& addr, byte_t* ptr, size_t sz)
          : inner{addr, _buf.data(), sz}
      {
        std::copy_n(ptr, sz, data());
      }
      constexpr byte_t*
      data()
      {
        return _buf.data();
      }
      constexpr const msghdr*
      send_hdr() const
      {
        return &inner.hdr;
      }

      void
      set_addr(const SockAddr& dst)
      {
        std::copy_n(
            reinterpret_cast<const byte_t*>(dst.operator const sockaddr*()),
            dst.sockaddr_len(),
            reinterpret_cast<byte_t*>(&inner.addr));
        inner.hdr.msg_namelen = dst.sockaddr_len();
      }

      void
      copy_buffer(const llarp_buffer_t& buf)
      {
        if (buf.sz > _buf.size())
          throw std::range_error{fmt::format("udp buffer too large: {} > {}", buf.sz, _buf.size())};
        inner.vec.iov_len = buf.sz;
        std::copy_n(buf.base, buf.sz, data());
      }
    };

    struct recv_buffer_t
    {
      static constexpr size_t buffer_size = ent_size - sizeof(buffer_inner);
      std::array<byte_t, buffer_size> buf{};
      buffer_inner inner{buf.data(), buf.size()};

      constexpr byte_t*
      data()
      {
        return buf.data();
      }
      constexpr msghdr*
      recv_hdr()
      {
        return &inner.hdr;
      }

      SockAddr
      addr() const
      {
        return SockAddr{(const sockaddr&)inner.addr};
      }
    };
#pragma pack(0)

    struct UDPSender : public Resource
    {
      UDPSocket& m_Sock;
      std::pmr::deque<send_buffer_t> m_SendQueue;
      bool m_Stop{false};
      UDPSender(UDPSocket& sock, Loop& loop)
          : Resource{loop}, m_Sock{sock}, m_SendQueue{&m_Sock.m_Mem} {};

      void
      completed(int st) override
      {
        log::debug(cat, "udp send completion: fd={} result={}", m_Sock.m_FD, st);
        m_SendQueue.pop_front();
        submit();
      };

      std::string_view
      name() const override
      {
        return "UDPSender";
      }

      void
      submit() override
      {
        if (m_SendQueue.empty())
          return;
        Submission sub{m_Loop, this};
        auto& front = m_SendQueue.front();
        sub.sendmsg(m_Sock.m_FD, front.send_hdr(), 0);
      }

      void
      shutdown() override
      {
        m_Stop = true;
      };

      bool
      done() const override
      {
        return false;  // we aren't added to the event loop.
      }

      bool
      should_wake_tickers() const override
      {
        return false;
      }

      bool
      sendmsg(const SockAddr& to, const llarp_buffer_t& pkt)
      {
        if (m_Stop)
          return false;
        auto& m_FD = m_Sock.m_FD;
        log::debug(cat, "sendmsg fd={} to={} sz={}", m_FD, to, pkt.sz);
        try
        {
          m_SendQueue.emplace_back(to, pkt.base, pkt.sz);
        }
        catch (std::bad_alloc&)
        {
          log::warning(cat, "std::bad_alloc while doing UDPSocket::sendmsg()");
          return false;
        }
        return true;
      }
    };
    std::shared_ptr<UDPSender> m_Sender;
    std::shared_ptr<EventLoopWakeup> m_SendWaker;
    SockAddr m_LocalAddr;
    std::unique_ptr<recv_buffer_t> m_RecvMsg{new recv_buffer_t{}};
    using buf_t = std::array<uint8_t, max_packets * recv_buffer_t::buffer_size>;
    std::unique_ptr<buf_t> m_Buf{new buf_t{}};
    std::pmr::monotonic_buffer_resource m_MemBuf{m_Buf->data(), m_Buf->size()};
    std::pmr::unsynchronized_pool_resource m_Mem{
        std::pmr::pool_options{1, recv_buffer_t::buffer_size}, &m_MemBuf};
    int m_FD{-1};
    bool m_Done{false};

   public:
    UDPSocket(Loop& loop, EventLoop::UDPReceiveFunc recv)
        : llarp::UDPHandle{std::move(recv)}, Resource{loop}
    {
      m_Sender = std::make_shared<UDPSender>(*this, loop);
      m_SendWaker = m_Loop.make_waker([sender = m_Sender]() { sender->submit(); });
    }

    bool
    listen(const SockAddr& addr) override
    {
      m_LocalAddr = addr;
      m_FD = ::socket(m_LocalAddr.Family(), SOCK_DGRAM, 0);
      if (m_FD == -1)
        return false;
      if (auto err =
              ::bind(m_FD, m_LocalAddr.operator const sockaddr*(), m_LocalAddr.sockaddr_len());
          err < 0)
      {
        llarp::log::error(cat, "failed to bind udp socket to {}: {}", m_LocalAddr, strerror(errno));
        errno = 0;
        return false;
      }
      submit();
      return true;
    }

    bool
    send(const SockAddr& dest, const llarp_buffer_t& buf) override
    {
      // in event loop thread, no need to make copy of buf.
      if (m_Loop.inEventLoop())
      {
        if (not m_Sender->sendmsg(dest, buf))
          return false;
        m_SendWaker->Trigger();
        return true;
      }
      // outside event loop thread, we have to copy buf.
      m_Loop.call([this, dest = SockAddr{dest}, buf = buf.copy()] {
        if (m_Sender->sendmsg(dest, buf))
          m_SendWaker->Trigger();
      });
      return true;
    }

    std::string_view
    name() const override
    {
      return "UDPSocket";
    }

    void
    submit() override
    {
      recvfrom();
    }

    void
    recvfrom()
    {
      m_RecvMsg->recv_hdr()->msg_namelen = m_LocalAddr.sockaddr_len();
      Submission sub{m_Loop, this};
      sub.recvmsg(m_FD, m_RecvMsg->recv_hdr(), 0);
    }

    void
    completed(int result) override
    {
      if (result < 0)
      {
        log::error(cat, "udp socket fd={} closed: {}", m_FD, strerror(0 - result));
        close();
        return;
      }
      auto* vec = m_RecvMsg->recv_hdr()->msg_iov;
      auto addr = m_RecvMsg->addr();
      log::debug(cat, "udp socket recvmsg fd={} from={} result={}", m_FD, addr, result);
      try
      {
        OwnedBuffer _pkt{reinterpret_cast<const byte_t*>(vec->iov_base), size_t(result), &m_Mem};
        on_recv(*this, std::move(addr), std::move(_pkt));
      }
      catch (std::bad_alloc&)
      {
        log::warning(cat, "std::bad_alloc while doing UDPSocket::recvfrom()");
      }
      submit();
    };

    bool
    should_wake_tickers() const override
    {
      return not m_Done;
    }

    std::optional<int>
    file_descriptor() override
    {
      if (m_FD == -1)
        return std::nullopt;
      return m_FD;
    }

    void
    close() override
    {
      m_Done = true;
      if (m_FD == -1)
        return;
      ::close(m_FD);
      m_FD = -1;
    }

    void
    shutdown() override
    {
      close();
    }

    std::optional<SockAddr>
    LocalAddr() const override
    {
      return m_LocalAddr;
    }

    bool
    done() const override
    {
      return m_Done;
    }
  };

  class Wakeup : public EventLoopWakeup, public Resource
  {
    std::array<int, 2> m_Pipe;
    std::function<void()> m_Callback;
    std::atomic_flag m_Flag;
    const bool m_Ticker;

   public:
    Wakeup(Loop& loop, std::function<void(void)> func, bool is_ticker)
        : Resource{loop}, m_Callback{std::move(func)}, m_Ticker{is_ticker}
    {
      pipe(m_Pipe.data());
      m_Flag.clear();
    }

    void
    completed(int result) override
    {
      if (result == -1)
      {
        if (m_Pipe[0] != -1)
          ::close(m_Pipe[0]);
        m_Pipe[0] = -1;
        return;
      }
      m_Flag.clear();
      if (m_Pipe[0] != -1)
      {
        int i{};
        ::read(m_Pipe[0], &i, sizeof(i));
        m_Callback();
      }
    }

    bool
    should_wake_tickers() const override
    {
      return not m_Ticker;
    }

    std::string_view
    name() const override
    {
      return "wakep";
    }

    bool
    done() const override
    {
      return m_Pipe[1] == -1 and m_Pipe[0] == -1;
    }

    void
    shutdown() override
    {
      if (m_Pipe[1] != -1)
        ::close(m_Pipe[1]);
      m_Pipe[1] = -1;
      if (m_Pipe[0] != -1)
        ::close(m_Pipe[0]);
      m_Pipe[0] = -1;
    }

    void
    Trigger() override
    {
      if (m_Flag.test_and_set())
        return;
      if (m_Pipe[1] == -1)
        return;
      int i{1};
      ::write(m_Pipe[1], &i, sizeof(i));
      submit();
    }

    void
    submit() override
    {
      if (m_Pipe[0] == -1)
        return;
      Submission sub{m_Loop, this};
      sub.poll(m_Pipe[0], POLL_IN);
    }
  };

  class Repeater : public EventLoopRepeater, public Resource
  {
    int m_FD;
    bool m_Oneshot;
    std::function<void(void)> m_Callback;

   public:
    Repeater(Loop& loop, bool oneshot = false)
        : Resource{loop}, m_FD{timerfd_create(CLOCK_MONOTONIC, 0)}, m_Oneshot{oneshot}
    {
      if (m_FD == -1)
        throw std::runtime_error{fmt::format("timerfd_create(): {}", strerror(errno))};
    }

    ~Repeater() override
    {
      if (m_FD != -1)
        ::close(m_FD);
    }

    std::string_view
    name() const override
    {
      return "repeater";
    }

    void
    start(Duration_t interval, std::function<void()> callback) override
    {
      const itimerspec timeout{to_timespec(interval), to_timespec(interval)};
      timerfd_settime(m_FD, 0, &timeout, nullptr);
      m_Callback = std::move(callback);
      submit();
    }

    void
    submit() override
    {
      if (m_FD == -1)
        return;
      Submission sub{m_Loop, this};
      sub.poll(m_FD, POLL_IN);
    }

    void
    completed(int res) override
    {
      if (res < 0)
      {
        shutdown();
        return;
      }
      uint64_t val;
      read(m_FD, &val, sizeof(val));
      m_Callback();
      if (m_Oneshot)
      {
        m_Loop.remove_handles_where([fd = m_FD](const auto& h) -> bool {
          if (auto self = std::dynamic_pointer_cast<Repeater>(h))
          {
            return self->m_FD == fd;
          }
          return false;
        });
        shutdown();
      }
      else
        submit();
    }

    bool
    should_wake_tickers() const override
    {
      return true;
    }

    void
    shutdown() override
    {
      if (m_FD == -1)
        return;
      ::close(m_FD);
      m_FD = -1;
    }

    bool
    done() const override
    {
      return m_FD == -1;
    }
  };

  namespace
  {
    template <typename Queue_t>
    void
    run_thread_worker(Queue_t* queue, std::string name)
    {
      llarp::util::SetThreadName(name);
      llarp::log::info(cat, "running {} thread", name);
      while (queue->enabled())
      {
        auto maybe = queue->popFrontWithTimeout(50ms);
        if (maybe)
        {
          log::debug(cat, "queue {} got entry", name);
          (*maybe)();
        }
      }
      llarp::log::info(cat, "ending {} thread", name);
    }
  }  // namespace

  Loop::Loop(size_t queue_size, size_t worker_threads)
      : llarp::EventLoop{}
      , m_EventLoopThreadID{std::nullopt}
      , m_LogicCalls{queue_size}
      , m_DiskCalls{128}
      , m_WorkCalls{256}
      , m_Now{llarp::time_now_ms()}
  {
    ::io_uring_queue_init(1024, &m_Ring, 0);
    m_Run = false;
    m_WorkerThreads.reserve(worker_threads);
  }

  llarp_time_t
  Loop::time_now() const
  {
    return m_Now;
  }

  bool
  Loop::inEventLoop() const
  {
    if (not m_EventLoopThreadID)
      return false;
    return *m_EventLoopThreadID == std::this_thread::get_id();
  }

  void
  Loop::wakeup()
  {
    m_LogicWaker->Trigger();
  }

  void
  Loop::io_wakeup() const
  {
    for (const auto& ticker : m_Tickers)
      ticker();
  }

  namespace
  {
    struct Work
    {
      EventLoopWork* _work;
      Loop* _loop;

      void
      operator()() const
      {
        _work->work();
        _loop->call_soon([w = _work]() {
          std::unique_ptr<EventLoopWork> work{w};
          work->cleanup(false);
          log::debug(cat, "work cleaned up");
        });
      }
    };
  }  // namespace

  void
  Loop::queue_work(std::unique_ptr<EventLoopWork> work)
  {
    log::debug(cat, "queue work");
    m_WorkCalls.pushBack(Work{work.release(), this});
  }

  void
  Loop::queue_slow_work(std::unique_ptr<EventLoopWork> work)
  {
    m_DiskCalls.pushBack(Work{work.release(), this});
  }

  bool
  Loop::add_ticker(std::function<void(void)> ticker)
  {
    call([self = this, ticker = std::move(ticker)]() {
      self->m_Tickers.emplace_back(std::move(ticker));
    });
    return true;
  }

  void
  Loop::call_soon(std::function<void(void)> f)
  {
    m_LogicCalls.pushBack(std::move(f));
    if (m_LogicWaker)
      m_LogicWaker->Trigger();
  }

  void
  Loop::call_later(llarp_time_t delay_ms, std::function<void()> callback)
  {
    call([self = this,
          h = std::make_shared<Repeater>(*this, true),
          callback = std::move(callback),
          delay_ms]() {
      h->start(delay_ms, std::move(callback));
      self->m_Handles.emplace_back(std::move(h));
    });
  }

  std::shared_ptr<EventLoopRepeater>
  Loop::make_repeater()
  {
    auto h = std::make_shared<Repeater>(*this);
    call([self = this, h]() {
      self->m_Timers.emplace_back(h);
      self->m_Handles.emplace_back(std::move(h));
    });
    return h;
  }

  std::shared_ptr<EventLoopWakeup>
  Loop::make_waker(std::function<void()> callback)
  {
    auto h = std::make_shared<Wakeup>(*this, std::move(callback), false);
    call([h, self = this]() { self->m_Handles.emplace_back(h); });
    return h;
  }

  void
  Loop::flush_logic()
  {
    while (not m_LogicCalls.empty())
    {
      auto job = m_LogicCalls.popFront();
      job();
    }
  }

  void
  Loop::run()
  {
    m_Run = true;
    m_EventLoopThreadID = std::this_thread::get_id();
    llarp::util::SetThreadName("llarpd-mainloop");

    m_LogicWaker = std::make_shared<Wakeup>(
        *this, [self = this]() { self->flush_logic(); }, false);
    m_TickerWaker = std::make_shared<Wakeup>(
        *this, [self = this]() { self->io_wakeup(); }, true);

    auto cleanup_handles = make_repeater();
    cleanup_handles->start(1s, [self = this]() {
      self->remove_handles_where([](auto h) {
        if (h)
          return h->done();
        return true;
      });
    });

    auto worker_threads = m_WorkerThreads.capacity();
    for (size_t idx = 0; idx < worker_threads; ++idx)
    {
      auto t = std::make_shared<std::thread>(
          [queue = &m_WorkCalls]() { run_thread_worker(queue, "llarpd-worker"); });
      m_WorkerThreads.push_back(t);
    }
    m_DiskThread = std::make_shared<std::thread>(
        [queue = &m_DiskCalls]() { run_thread_worker(queue, "llarpd-disk"); });

    m_LogicWaker->Trigger();
    std::vector<std::shared_ptr<Wakeup>> wakeups;
    std::array<io_uring_cqe*, 128> events{};
    do
    {
      auto timeout = as_timespec(100ms);
      io_uring_cqe* cqe{nullptr};
      if (auto err = io_uring_wait_cqe_timeout(ring(), &cqe, &timeout))
      {
        if (err != 0 - ETIMEDOUT)
        {
          llarp::log::error(cat, "io_uring_wait_cqe_timeout(): {}", strerror(0 - err));
          break;
        }
      }
      m_Now = llarp::time_now_ms();
      if (cqe)
      {
        size_t num_events = io_uring_peek_batch_cqe(ring(), events.data(), events.size());
        log::debug(cat, "got {} events", num_events);
        for (size_t idx = 0; idx < num_events; ++idx)
        {
          cqe = events[idx];
          auto resource =
              reinterpret_cast<Resource*>(io_uring_cqe_get_data(cqe))->shared_from_this();
          log::debug(cat, "event loop pump from {} res={}", resource->name(), cqe->res);

          // trigger tickers.
          if (resource->should_wake_tickers())
          {
            m_TickerWaker->Trigger();
          }
          // defer execution if this is a wakeup.
          if (auto wakeup_ptr = std::dynamic_pointer_cast<Wakeup>(resource))
          {
            wakeups.emplace_back(std::move(wakeup_ptr));
          }
          else
          {
            resource->completed(cqe->res);
          }
          io_uring_cqe_seen(ring(), cqe);
          if (resource->done())
          {
            remove_handles_where([resource](auto h) { return h.get() == resource.get(); });
          }
        }
        // check for no more events to call wakeups.
        if (io_uring_peek_batch_cqe(ring(), &cqe, 1) == 0)
        {
          std::shared_ptr<Wakeup> ticker{nullptr};
          for (auto& waker : wakeups)
          {
            // if this is a normal waker call its completion handler otherwise defer calling it.
            if (waker->should_wake_tickers())
              waker->completed(1);
            else
              ticker = std::move(waker);
          }
          wakeups.clear();
          // call ticker wakeup last.
          if (ticker)
            ticker->completed(1);
        }
      }
      flush_logic();
    } while (m_Run.load() and not m_Handles.empty());

    m_WorkCalls.disable();
    for (auto& worker : m_WorkerThreads)
    {
      if (worker)
        worker->join();
    }

    m_WorkerThreads.clear();

    m_DiskCalls.disable();
    if (m_DiskThread)
    {
      m_DiskThread->join();
      m_DiskThread = nullptr;
    }

    for (auto& handle : m_Handles)
    {
      if (handle)
        handle->shutdown();
    }
    m_Handles.clear();
    m_Tickers.clear();

    m_LogicWaker = nullptr;
    m_TickerWaker = nullptr;
  }

  std::shared_ptr<UDPHandle>
  Loop::make_udp(UDPReceiveFunc func)
  {
    auto h = std::make_shared<UDPSocket>(*this, func);
    call([self = this, h]() { self->m_Handles.emplace_back(std::move(h)); });
    return h;
  }

  std::shared_ptr<EventLoopPoller>
  Loop::add_poller(int fd, std::function<void()> callback)
  {
    auto h = std::make_shared<Poller>(*this, fd, std::move(callback));
    call([self = this, h]() {
      self->m_Handles.emplace_back(h);
      h->submit();
    });
    return h;
  }

  bool
  Loop::add_network_interface(
      std::shared_ptr<llarp::vpn::NetworkInterface> netif,
      std::function<void(llarp::net::IPPacket)> handler)
  {
    int fd = netif->PollFD();
    log::info(cat, "adding network interface fd={}", fd);
    auto& on_stop = netif->on_stop;
    auto poller = add_poller(fd, [netif = std::move(netif), handler = std::move(handler)]() {
      do
      {
        auto pkt = netif->ReadNextPacket();
        if (pkt.empty())
          return;
        handler(std::move(pkt));
        netif->MaybeWakeUpperLayers();
      } while (true);
    });
    on_stop = [poller = std::move(poller)]() { poller->close(); };
    return true;
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  void
  Loop::stop()
  {
    for (auto handle : m_Timers)
      handle->shutdown();
    m_Timers.clear();
    m_Run = false;
  }

  size_t
  Loop::num_worker_threads() const
  {
    return m_WorkerThreads.capacity();
  }
}  // namespace llarp::io_uring
