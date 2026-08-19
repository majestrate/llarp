#ifdef WITH_LIBUV
#include <llarp/util/alloc.h>
#include "libuv.hpp"
#include "tcp_handle.hpp"

#include <system_error>
#include <uv.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <cstring>
#include <unistd.h>
#include <netinet/udp.h>

#include <llarp/util/thread/queue.hpp>
#include <llarp/vpn/platform.hpp>
#include <llarp/util/fd.hpp>
#include <uvw.hpp>

#ifdef __APPLE__
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
struct mmsghdr
{
  struct msghdr msg_hdr;
  unsigned int msg_len;
};
static int
sendmmsg(int fd, struct mmsghdr* msgvec, unsigned int vlen, int flags)
{
  unsigned int sent = 0;
  for (; sent < vlen; sent++)
  {
    const ssize_t ret = ::sendmsg(fd, &msgvec[sent].msg_hdr, flags);
    if (ret < 0)
      return sent ? static_cast<int>(sent) : -1;
    msgvec[sent].msg_len = static_cast<unsigned int>(ret);
  }
  return static_cast<int>(sent);
}
#endif  // __APPLE__

namespace llarp::uv
{
  static auto logcat = log::Cat("libuv");

  std::shared_ptr<uvw::Loop>
  Loop::MaybeGetUVWLoop()
  {
    return m_Impl;
  }

  class TCPConnectionImpl : public TCPConnection
  {
    ::uv_tcp_t m_Handle;
    SockAddr m_LocalAddr{}, m_RemoteAddr{};

    static void
    OnClose(uv_handle_t* handle)
    {
      static_cast<TCPConnectionImpl*>(handle->data)->Untrack();
    }

    static void
    OnSent(uv_write_t* req, int status)
    {
      auto* r = static_cast<SendRequest*>(req->data);
      if (status)
        log::warning(logcat, "TCPConnection::Send() write failed: {}", uv_strerror(status));
      r->Completion(status);
      delete r;
    }
    static void
    ReadAlloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
    {
      buf->base = new char[suggested_size];
    }

    static void
    OnRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
    {
      const std::unique_ptr<char[]> ptr{buf->base};
      auto* self = static_cast<TCPConnectionImpl*>(handle->data);

      if (nread < 0)
      {
        self->ReadEnded();
        return;
      }
      if (nread == 0)
      {
        // empty buffer.
        self->m_RecvHandler(std::nullopt, std::error_code{});
        return;
      }
      // copy buffer.
      const size_t sz = nread;
      self->m_RecvHandler(
          OwnedBuffer{reinterpret_cast<const byte_t*>(ptr.get()), sz}, std::error_code{});
    }

    void
    ReadEnded()
    {
      m_RecvHandler(std::nullopt, std::error_code{ESHUTDOWN, std::generic_category()});
      Close();
    }

   public:
    friend TCPConnectionPoolImpl;
    friend TCPAcceptorImpl;
    TCPConnectionImpl(uv_loop_t* loop, RecvHandler h, TCPConnectionPool& pool)
        : TCPConnection{h, pool}
    {
      if (auto ret = ::uv_tcp_init(loop, &m_Handle); ret < 0)
        throw std::runtime_error{fmt::format("uv_tcp_init(): {}", uv_strerror(ret))};
      m_Handle.data = this;
    }

    SockAddr
    LocalAddr() const override
    {
      return m_LocalAddr;
    }

    SockAddr
    RemoteAddr() const override
    {
      return m_RemoteAddr;
    }

    void
    Close() override
    {
      ::uv_close((::uv_handle_t*)&m_Handle, &OnClose);
    }

    int
    Bind(SockAddr laddr) override
    {
      if (auto err = ::uv_tcp_bind(&m_Handle, laddr.operator const sockaddr*(), 0); err != 0)
        return err;

      m_LocalAddr = laddr;
      return 0;
    }

    bool
    Start() override
    {
      if (auto err = ::uv_read_start((::uv_stream_t*)&m_Handle, &ReadAlloc, &OnRead); err != 0)
      {
        log::error(logcat, "TCPConnection::Start() uv_read_start(): {}", uv_strerror(err));
        return false;
      }
      return true;
    }

    void
    Send(OwnedBuffer buffer, SendCompletionHandler completionHandler) override
    {
      uv_write_t* req = new uv_write_t{};
      SendRequest* sendr = new SendRequest{std::move(buffer), std::move(completionHandler), req};
      req->data = sendr;
      ::uv_buf_t buf = uv_buf_init(sendr->ptr(), sendr->size());
      if (auto ret = ::uv_write(req, (::uv_stream_t*)&m_Handle, &buf, 1, &OnSent); ret < 0)
      {
        sendr->Completion(ret);
        delete sendr;
      }
    }

   private:
    struct SendRequest
    {
      OwnedBuffer buffer;
      SendCompletionHandler completed;
      uv_write_t* const req;

      char*
      ptr() const noexcept
      {
        return reinterpret_cast<char*>(buffer.buf.get());
      }

      constexpr size_t
      size() const noexcept
      {
        return buffer.sz;
      }

      void
      Completion(int status)
      {
        completed(std::error_code(status, std::generic_category()));
        delete req;
      }
    };
  };

  class TCPAcceptorImpl : public TCPAcceptor
  {
    uv_tcp_t m_Handle;
    std::optional<SockAddr> m_LocalAddr;

    static void
    OnNewConn(uv_stream_t* handle, int status)
    {
      auto* self = static_cast<TCPAcceptorImpl*>(handle->data);
      if (status)
      {
        self->m_AcceptHandler(nullptr, std::error_code{status, std::generic_category()});
        return;
      }
      auto conn = std::static_pointer_cast<TCPConnectionImpl>(self->MakeConn());
      if (auto err = ::uv_accept(handle, (::uv_stream_t*)&conn->m_Handle); err != 0)
      {
        log::error(logcat, "TCPAcceptor: uv_accept(): {}", uv_strerror(err));
        conn->Untrack();
        self->m_AcceptHandler(nullptr, std::error_code{err, std::generic_category()});
        return;
      }
      self->m_AcceptHandler(std::move(conn), std::error_code{});
    }

    static void
    OnClose(uv_handle_t* h)
    {
      auto* self = static_cast<TCPAcceptorImpl*>(h->data);
      self->Untrack();
    }

   public:
    TCPAcceptorImpl(Loop& loop, AcceptHandler handler, TCPConnectionPool& pool)
        : TCPAcceptor{handler, pool}
    {
      if (auto err = ::uv_tcp_init(loop.m_Impl->raw(), &m_Handle); err != 0)
        throw std::runtime_error{fmt::format("TCPAcceptor() uv_tcp_init(): {}", uv_strerror(err))};
    }

    bool
    Bind(SockAddr addr) override
    {
      if (auto err = ::uv_tcp_bind(&m_Handle, addr.operator const sockaddr*(), 0); err != 0)
      {
        log::error(logcat, "TCPAcceptor::Bind() uv_tcp_bind() failed: {}", uv_strerror(err));
        return false;
      }

      if (auto err = ::uv_listen((::uv_stream_t*)&m_Handle, 5, &OnNewConn); err != 0)
      {
        log::error(logcat, "TCPAcceptor::Bind() uv_listen() failed: {}", uv_strerror(err));
        return false;
      }

      m_LocalAddr = addr;
      return true;
    }

    std::optional<SockAddr>
    LocalAddr() const override
    {
      return m_LocalAddr;
    }

    void
    Close() override
    {
      ::uv_close((uv_handle_t*)&m_Handle, &OnClose);
    }
  };

  class TCPConnectionPoolImpl : public TCPConnectionPool
  {
    Loop& m_Loop;

    struct ConnectContext
    {
      std::shared_ptr<TCPConnectionImpl> conn;
      CompletionHandler handler;
      uv_connect_t* const m_Req;

      ~ConnectContext()
      {
        delete m_Req;
      }
    };

    static void
    OnConnectResult(uv_connect_t* req, int status)
    {
      ConnectContext* ctx = reinterpret_cast<ConnectContext*>(req->data);
      if (not ctx)
        return;
      ;
      ctx->handler(ctx->conn, std::error_code{status, std::generic_category()});
      delete ctx;
    }

    void
    AsyncConnect(
        std::shared_ptr<TCPConnectionImpl> conn, SockAddr remote, CompletionHandler handler)
    {
      conn->m_RemoteAddr = remote;
      ::uv_connect_t* req = new uv_connect_t{};
      req->data = new ConnectContext{conn, std::move(handler), req};
      if (auto ret = ::uv_tcp_connect(
              req, &conn->m_Handle, remote.operator const sockaddr*(), &OnConnectResult);
          ret < 0)
      {
        delete static_cast<ConnectContext*>(req->data);
        delete req;
        throw std::runtime_error{fmt::format("uv_tcp_connect(): {}", ::uv_strerror(ret))};
      }
    }

   public:
    friend TCPAcceptorImpl;

    TCPConnectionPoolImpl(Loop& loop) : m_Loop{loop}
    {}

    ~TCPConnectionPoolImpl() override = default;

    void
    Connect(
        SockAddr remote_addr,
        CompletionHandler completion_hander,
        TCPConnection::RecvHandler recv_handler,
        std::optional<SockAddr> local_addr) override
    {
      if (auto conn_ptr = std::static_pointer_cast<TCPConnectionImpl>(
              MakeConnection(local_addr, std::move(recv_handler))))
      {
        AsyncConnect(std::move(conn_ptr), std::move(remote_addr), std::move(completion_hander));
        return;
      }
      int err = errno;
      errno = 0;
      completion_hander(nullptr, std::error_code{err, std::generic_category()});
    }

    std::shared_ptr<TCPConnection>
    MakeConnection(std::optional<SockAddr> local, TCPConnection::RecvHandler recv_handler) override
    {
      auto conn =
          std::make_shared<TCPConnectionImpl>(m_Loop.m_Impl->raw(), std::move(recv_handler), *this);
      if (local)
      {
        if (auto err = conn->Bind(*local); err < 0)
        {
          log::error(logcat, "TCPConnection::Bind() failed: {}", uv_strerror(err));
          errno = err;
          return nullptr;
        }
      }
      return std::static_pointer_cast<TCPConnection>(conn);
    }

    std::shared_ptr<TCPAcceptor>
    CreateAcceptor(AcceptHandler accept_handler) override
    {
      auto acceptor = std::make_shared<TCPAcceptorImpl>(m_Loop, std::move(accept_handler), *this);
      return std::static_pointer_cast<TCPAcceptor>(acceptor);
    }

    void
    CloseAll() override
    {
      for (const auto& [addr, conn] : m_Connections)
      {
        conn->Close();
      }
    }

   protected:
    std::shared_ptr<TCPConnection>
    MakeConn()
    {
      return MakeConnection(std::nullopt, [](std::optional<OwnedBuffer>, std::error_code) {});
    }
  };

  class UVWakeup final : public EventLoopWakeup
  {
    std::shared_ptr<uvw::AsyncHandle> async;

   public:
    UVWakeup(uvw::Loop& loop, std::function<void()> callback)
        : async{loop.resource<uvw::AsyncHandle>()}
    {
      async->on<uvw::AsyncEvent>([f = std::move(callback)](auto&, auto&) { f(); });
    }

    void
    Trigger() override
    {
      async->send();
    }

    ~UVWakeup() override
    {
      async->close();
    }
  };

  class UVRepeater final : public EventLoopRepeater
  {
    std::shared_ptr<uvw::TimerHandle> timer;

   public:
    UVRepeater(uvw::Loop& loop) : timer{loop.resource<uvw::TimerHandle>()}
    {}

    void
    start(llarp_time_t every, std::function<void()> task) override
    {
      timer->start(every, every);
      timer->on<uvw::TimerEvent>([task = std::move(task)](auto&, auto&) { task(); });
    }

    ~UVRepeater() override
    {
      timer->stop();
    }
  };

  struct UDPHandle final : llarp::UDPHandle
  {
    UDPHandle(Loop&, ReceiveFunc rf);

    bool
    listen(const SockAddr& addr) override;

    bool
    send(const SockAddr& dest, const llarp_buffer_t& buf) override
    {
      m_Loop.call([this,
                   dest = SockAddr{dest},
                   buffer = std::make_shared<OwnedBuffer>(buf.base, buf.sz)]() {
        OwnedBuffer& buf = *buffer;
        m_SendQueue.emplace_back(std::make_pair(dest, std::move(buf)));
        m_SendWakeup->Trigger();
      });
      return true;
    }

    std::optional<SockAddr>
    LocalAddr() const override
    {
      if (file_descriptor() == std::nullopt)
        return std::nullopt;
      return m_LocalAddr;
    }

    std::optional<int>
    file_descriptor() const override
    {
      if (not m_FD)
        return std::nullopt;
      if (int fd = m_FD->fd(); fd >= 0)
        return fd;
      return std::nullopt;
    }

    void
    close() override;

    ~UDPHandle() override;

   private:
    // (remote_addr, data)
    using Event_t = std::pair<SockAddr, OwnedBuffer>;
    Loop& m_Loop;
    std::unique_ptr<util::FD> m_FD;
    SockAddr m_LocalAddr{};

    std::vector<std::jthread> m_ReaderThreads;
    thread::Queue<Event_t> m_Gather;

    std::vector<Event_t> m_SendQueue;
    std::shared_ptr<EventLoopWakeup> m_SendWakeup, m_RecvWakeup;

    void
    reset_handle(int num_threads)
    {
      if (num_threads <= 0)
        num_threads = std::thread::hardware_concurrency();

      close();

      // create send flusher
      if (m_SendWakeup == nullptr)
      {
        m_SendWakeup = m_Loop.make_waker([this]() {
          std::vector<::mmsghdr> msgs{};
          std::vector<::iovec> send_vecs;
          send_vecs.resize(m_SendQueue.size());
          size_t idx{};
          const int send_flags{MSG_DONTWAIT | MSG_NOSIGNAL};
          for (auto& [addr, pkt] : m_SendQueue)
          {
            auto& msg = msgs.emplace_back();
            auto& hdr = msg.msg_hdr;
            hdr.msg_name =
                const_cast<void*>(reinterpret_cast<const void*>(addr.operator const sockaddr*()));
            hdr.msg_namelen = addr.sockaddr_len();
            hdr.msg_control = nullptr;
            hdr.msg_controllen = 0;
            hdr.msg_flags = send_flags;

            ::iovec* start = &send_vecs[idx];

            auto& vec = send_vecs[idx];
            vec.iov_base = pkt.buf.get();
            vec.iov_len = pkt.sz;
            ++idx;

            hdr.msg_iov = start;
            hdr.msg_iovlen = 1;
          }
          int ret{};
          log::debug(
              logcat,
              "UDPHandle sendmmsg(): send {} addrs {} vecs | sendq={}",
              msgs.size(),
              idx,
              m_SendQueue.size());

          if (auto maybe_fd = file_descriptor())
          {
            if (ret = ::sendmmsg(*maybe_fd, msgs.data(), msgs.size(), send_flags); ret == -1)
            {
              log::error(logcat, "UPDHandle sendmmsg(): ", strerror(errno));
              errno = 0;
            }
            log::debug(logcat, "sendmmsg(): {}", ret);
          }
          else
            log::warning(logcat, "sendmmsg(): no file descriptor");

          m_SendQueue.clear();
        });
      }

      // create recv handler.
      if (m_RecvWakeup == nullptr)
      {
        m_RecvWakeup = m_Loop.make_waker([this]() {
          const bool should_skip = not(bool{on_recv} and m_Gather.enabled());
          while (auto maybe = m_Gather.tryPopFront())
          {
            log::debug(logcat, "process {}B from {}", maybe->second.sz, maybe->first);
            if (should_skip)
              continue;
            on_recv(*this, maybe->first, std::move(maybe->second));
          }
          m_Loop.io_cycle_complete();
        });
      }

      // spawn reader threads.
      m_Gather.enable();
      while (num_threads > 0)
      {
        num_threads--;

        m_ReaderThreads.emplace_back([this]() {
          util::SetThreadName("llarp-recvmsg");
          const int recv_flags{};
          msghdr msg{};
          using Buffer_t = std::array<uint8_t, 1500>;
          Buffer_t recv_buffer{};
          ::iovec recv_vec{};
          sockaddr_storage recv_addr{};

          while (m_Gather.enabled())
          {
            auto maybe_fd = file_descriptor();
            if (not maybe_fd)
            {
              log::debug(logcat, "UDPHandle: no fd, sleeping");
              std::this_thread::sleep_for(100ms);
              continue;
            }

            msg.msg_control = 0;
            msg.msg_controllen = 0;
            msg.msg_flags = 0;
            msg.msg_namelen = 0;
            msg.msg_iov = &recv_vec;
            msg.msg_iovlen = 1;
            msg.msg_name = &recv_addr;
            msg.msg_namelen = sizeof(sockaddr_storage);
            msg.msg_iov->iov_base = recv_buffer.data();
            msg.msg_iov->iov_len = recv_buffer.size();

            int ret{};
            if (ret = ::recvmsg(*maybe_fd, &msg, recv_flags); ret == -1)
            {
              int err = errno;
              errno = 0;
              if (err == EAGAIN or err == EWOULDBLOCK)
                continue;
              log::error(logcat, "UPDHandle recvmsg(): {}", strerror(err));
              continue;
            }
            log::debug(logcat, "recvmsg(): {}", ret);

            if (ret <= 0)
            {
              log::debug(logcat, "UPDHandle ret: {} <= 0", ret);
              continue;
            }
            auto& hdr = msg;
            if (hdr.msg_namelen == 0)
            {
              log::debug(logcat, "UPDHandle msg_namelen: {} <= 0", hdr.msg_namelen);
              continue;
            }
            sockaddr* from_ptr = reinterpret_cast<sockaddr*>(hdr.msg_name);
            if (from_ptr == nullptr)
            {
              log::debug(logcat, "UPDHandle from_ptr == nullptr");
              continue;
            }
            m_RecvWakeup->Trigger();
            SockAddr from{*from_ptr};
            OwnedBuffer pkt{static_cast<byte_t*>(hdr.msg_iov->iov_base), static_cast<size_t>(ret)};
            auto result = m_Gather.tryPushBack(std::make_pair(from, std::move(pkt)));
            if (result == thread::QueueReturn::QueueDisabled)
            {
              log::debug(logcat, "UPDHandle m_Gater has been disabled");
            }
          }
          log::info(logcat, "UDP worker ended");
        });
      }
    }
  };

  void
  run_disk_thread(void*);

  void
  run_worker_thread(void*);

  void
  Loop::FlushLogic()
  {
    llarp::LogTrace("Loop::FlushLogic() start");
    while (not m_LogicCalls.empty())
    {
      auto f = m_LogicCalls.popFront();
      f();
    }
    llarp::LogTrace("Loop::FlushLogic() end");
  }

  void
  Loop::io_cycle_complete()
  {
    for (auto& ticker : m_tickers)
      ticker();
  }

  void
  Loop::tick_event_loop()
  {
    llarp::LogTrace("ticking event loop.");
    FlushLogic();
  }

  Loop::Loop(size_t queue_size, size_t worker_threads)
      : llarp::EventLoop{}
      , m_LogicCalls{queue_size}
      , m_DiskCalls{128}
      , m_WorkCalls{512 * worker_threads}
  {
    if (!(m_Impl = uvw::Loop::create()))
      throw std::runtime_error{"Failed to construct libuv loop"};

#ifdef LOKINET_DEBUG
    last_time = 0;
    loop_run_count = 0;
#endif

    signal(SIGPIPE, SIG_IGN);

    m_ConnectionPool = std::make_shared<TCPConnectionPoolImpl>(*this);

    m_Run.store(true);
    m_nextID.store(0);
    if (!(m_WakeUp = m_Impl->resource<uvw::AsyncHandle>()))
      throw std::runtime_error{"Failed to create libuv async"};
    m_WakeUp->on<uvw::AsyncEvent>([this](const auto&, auto&) { tick_event_loop(); });
    m_DiskThread =
        std::make_unique<std::thread>([queue = &m_DiskCalls]() { run_disk_thread(queue); });

    while (worker_threads > 0)
    {
      m_WorkThreads.emplace_back([queue = &m_WorkCalls]() { run_worker_thread(queue); });
      worker_threads--;
    }
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  void
  Loop::run()
  {
    llarp::LogTrace("Loop::run_loop()");
    m_EventLoopThreadID = std::this_thread::get_id();
    m_Impl->run();
    m_Impl->close();

    for (auto& closer : m_closers)
      closer();
    m_closers.clear();

    m_DiskCalls.disable();
    if (m_DiskThread and m_DiskThread->joinable())
      m_DiskThread->join();
    m_DiskThread.reset();
    m_WorkCalls.disable();
    for (auto& t : m_WorkThreads)
    {
      if (t.joinable())
        t.join();
    }
    m_WorkThreads.clear();
    m_Impl.reset();
    llarp::LogInfo("we have stopped");
  }

  void
  Loop::wakeup()
  {
    m_WakeUp->send();
  }

  std::shared_ptr<llarp::UDPHandle>
  Loop::make_udp(UDPReceiveFunc on_recv)
  {
    return std::static_pointer_cast<llarp::UDPHandle>(
        std::make_shared<llarp::uv::UDPHandle>(*this, std::move(on_recv)));
  }

  static void
  setup_oneshot_timer(uvw::Loop& loop, llarp_time_t delay, std::function<void()> callback)
  {
    auto timer = loop.resource<uvw::TimerHandle>();
    timer->on<uvw::TimerEvent>([f = std::move(callback)](const auto&, auto& timer) {
      f();
      timer.stop();
      timer.close();
    });
    timer->start(delay, 0ms);
  }

  void
  Loop::call_later(llarp_time_t delay_ms, std::function<void(void)> callback)
  {
    llarp::LogTrace("Loop::call_after_delay()");
#ifdef TESTNET_SPEED
    delay_ms *= TESTNET_SPEED;
#endif

    if (inEventLoop())
      setup_oneshot_timer(*m_Impl, delay_ms, std::move(callback));
    else
    {
      call_soon([this, f = std::move(callback), target_time = time_now() + delay_ms] {
        // Recalculate delay because it may have taken some time to get ourselves into the logic
        // thread
        auto updated_delay = target_time - time_now();
        if (updated_delay <= 0ms)
          f();  // Timer already expired!
        else
          setup_oneshot_timer(*m_Impl, updated_delay, std::move(f));
      });
    }
  }

  Loop::~Loop()
  {
    llarp::EventLoop::~EventLoop();
  }

  void
  Loop::add_closer(std::function<void()> f)
  {
    m_closers.emplace_back(std::move(f));
  }

  void
  Loop::stop()
  {
    if (!m_Run)
      return;

    if (not inEventLoop())
      return call_soon([this] { stop(); });

    llarp::LogInfo("stopping event loop");
    m_Impl->walk([](auto&& handle) {
      if constexpr (!std::is_pointer_v<std::remove_reference_t<decltype(handle)>>)
        handle.close();
    });

    for (auto& closer : m_closers)
      closer();

    m_closers.clear();

    llarp::LogDebug("Closed all handles, stopping the loop");
    m_Impl->stop();

    m_WorkCalls.disable();

    for (auto& t : m_WorkThreads)
    {
      if (t.joinable())
        t.join();
    }
    m_WorkThreads.clear();

    if (m_DiskThread)
    {
      m_DiskCalls.disable();
      if (m_DiskThread->joinable())
        m_DiskThread->join();
      m_DiskThread.reset();
    }

    m_Run.store(false);
  }

  bool
  Loop::add_ticker(std::function<void(void)> func)
  {
    m_tickers.push_back(func);
    auto check = m_Impl->resource<uvw::CheckHandle>();
    check->on<uvw::CheckEvent>([f = std::move(func)](auto&, auto&) { f(); });
    check->start();
    return true;
  }

  bool
  Loop::add_network_interface(
      std::shared_ptr<llarp::vpn::NetworkInterface> netif,
      std::function<void(llarp::net::IPPacket)> handler)
  {
    using event_t = uvw::PollEvent;
    auto handle = m_Impl->resource<uvw::PollHandle>(netif->PollFD());
    if (!handle)
      return false;

    handle->on<event_t>([netif = std::move(netif), handler = std::move(handler)](
                            const event_t&, [[maybe_unused]] auto& handle) {
      for (auto pkt = netif->ReadNextPacket(); true; pkt = netif->ReadNextPacket())
      {
        if (pkt.empty())
          return;
        if (handler)
          handler(std::move(pkt));
        // on windows/apple, vpn packet io does not happen as an io action that wakes up the event
        // loop thus, we must manually wake up the event loop when we get a packet on our interface.
        // on linux/android this is a nop
        netif->MaybeWakeUpperLayers();
      }
    });

    handle->start(uvw::PollHandle::Event::READABLE);

    return true;
  }

  void
  Loop::call_soon(std::function<void(void)> f)
  {
    if (not m_EventLoopThreadID.has_value())
    {
      m_LogicCalls.tryPushBack(f);
      m_WakeUp->send();
      return;
    }

    if (inEventLoop() and m_LogicCalls.full())
    {
      FlushLogic();
    }
    m_LogicCalls.pushBack(f);
    m_WakeUp->send();
  }

  llarp::uv::UDPHandle::UDPHandle(Loop& loop, ReceiveFunc rf)
      : llarp::UDPHandle{std::move(rf)}, m_Loop{loop}, m_Gather{128}
  {
    loop.add_closer([this]() { close(); });
    reset_handle(loop.num_worker_threads());
  }

  bool
  UDPHandle::listen(const SockAddr& addr)
  {
    int fd = ::socket(addr.Family(), SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
      return false;
    if (::bind(fd, addr.operator const sockaddr*(), addr.sockaddr_len()) == -1)
    {
      ::close(fd);
      return false;
    }

    const timeval timeout{
        .tv_sec = 0,
        .tv_usec = 100 * 1000,
    };

    if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1)
    {
      ::close(fd);
      return false;
    }

    sockaddr_storage sas{};
    sockaddr* sa = reinterpret_cast<sockaddr*>(&sas);
    socklen_t sa_len = sizeof(sas);
    if (::getsockname(fd, sa, &sa_len) != 0)
    {
      ::close(fd);
      return false;
    }

    m_FD = std::make_unique<util::FD>(fd);
    m_LocalAddr = SockAddr{*sa};
    return true;
  }

  void
  UDPHandle::close()
  {
    // stop reading
    m_Gather.disable();
    log::debug(logcat, "UDPHandle close(): disable m_Gather");
    // clear fd
    m_FD.reset();
    log::debug(logcat, "UDPHandle close(): m_FD closed");
    // join reader threads
    size_t joined{};
    for (auto& th : m_ReaderThreads)
    {
      if (th.joinable())
      {
        joined++;
        log::debug(logcat, "UDPHandle close(): join {}/{}", joined, m_ReaderThreads.size());
        th.join();
      }
    }
    log::debug(logcat, "UDPHandle close(): joined {}/{} threads", joined, m_ReaderThreads.size());
    m_ReaderThreads.clear();
    log::debug(logcat, "UDPHandle close(): m_ReadThreads.clear() complete");
  }

  UDPHandle::~UDPHandle()
  {
    m_Gather.disable();
    m_FD.reset();
    for (auto& th : m_ReaderThreads)
    {
      if (th.joinable())
        th.join();
    }
  }

  std::shared_ptr<llarp::EventLoopWakeup>
  Loop::make_waker(std::function<void()> callback)
  {
    return std::static_pointer_cast<llarp::EventLoopWakeup>(
        std::make_shared<UVWakeup>(*m_Impl, std::move(callback)));
  }

  std::shared_ptr<EventLoopRepeater>
  Loop::make_repeater()
  {
    return std::static_pointer_cast<EventLoopRepeater>(std::make_shared<UVRepeater>(*m_Impl));
  }

  bool
  Loop::inEventLoop() const
  {
    if (m_EventLoopThreadID)
      return *m_EventLoopThreadID == std::this_thread::get_id();
    // assume we are in it because we haven't started up yet
    return true;
  }

  void
  run_worker_thread(void* arg)
  {
    using Queue_t = llarp::thread::Queue<std::function<void(void)>>;
    llarp::util::SetThreadName("llarp-worker");
    auto* queue = reinterpret_cast<Queue_t*>(arg);
    while (queue->enabled())
    {
      auto maybe = queue->popFrontWithTimeout(1s);
      if (maybe)
        maybe.value()();
    }
  }

  void
  Loop::queue_work(std::unique_ptr<EventLoopWork> ev_work)
  {
    if (not m_WorkCalls.enabled())
      return;
    m_WorkCalls.pushBack([work = std::shared_ptr<EventLoopWork>(ev_work.release()), self = this]() {
      work->work();
      self->call_soon([work]() { work->cleanup(false); });
    });
  }

  void
  run_disk_thread(void* arg)
  {
    using Queue_t = llarp::thread::Queue<std::function<void(void)>>;
    llarp::util::SetThreadName("llarp-disk");
    LogInfo("Disk worker started");
    auto* queue = reinterpret_cast<Queue_t*>(arg);
    while (queue->enabled())
    {
      auto maybe = queue->popFrontWithTimeout(1s);
      if (maybe)
        maybe.value()();
    }
    LogInfo("Disk worker ended");
  }

  void
  Loop::queue_slow_work(std::unique_ptr<EventLoopWork> work)
  {
    if (not m_DiskCalls.enabled())
      return;
    m_DiskCalls.pushBack([work = std::shared_ptr<EventLoopWork>(work.release()), self = this]() {
      work->work();
      self->call_soon([work]() { work->cleanup(false); });
    });
  }

  class Poller : public EventLoopPoller
  {
    struct Impl
    {
      Poller* const m_Parent;
      std::function<void()> m_Callback;
      uv_poll_t m_Poller;

      static void
      on_poll(uv_poll_t* h, int status, int)
      {
        auto* self = reinterpret_cast<Poller::Impl*>(h->data);
        if (status == UV_EBADF)
        {
          self->m_Parent->close();
        }
        if (status >= 0)
          self->m_Callback();
      }

      static void
      on_closed(uv_handle_t* h)
      {
        auto* self = reinterpret_cast<Poller::Impl*>(h->data);
        self->m_Parent->m_Impl.reset();
      }

      explicit Impl(Poller* parent, std::function<void()> cb)
          : m_Parent{parent}, m_Callback{std::move(cb)}
      {
        m_Poller.data = this;
      }
    };

    std::unique_ptr<Impl> m_Impl;

    auto*
    poller()
    {
      return &m_Impl->m_Poller;
    }
    uv_handle_t*
    handle()
    {
      return (uv_handle_t*)poller();
    }

   public:
    Poller(uvw::Loop& loop, int fd, std::function<void()> callback)
        : m_Impl{std::make_unique<Impl>(this, std::move(callback))}
    {
      auto* loop_ptr = loop.raw();
      uv_poll_init(loop_ptr, poller(), fd);
      if (auto err = uv_poll_start(poller(), UV_READABLE, &Impl::on_poll); err < 0)
      {
        throw std::runtime_error{fmt::format("uv_poll_start(): {}", uv_strerror(err))};
      }
    }

    ~Poller() override
    {
      if (m_Impl)
        uv_poll_stop(poller());
    }

    void
    close() override
    {
      if (not m_Impl)
        return;
      uv_poll_stop(poller());
      uv_close(handle(), &Impl::on_closed);
    }
  };

  std::shared_ptr<EventLoopPoller>
  Loop::add_poller(int fd, std::function<void()> callback)
  {
    return std::make_shared<Poller>(*m_Impl, fd, std::move(callback));
  }

  size_t
  Loop::num_worker_threads() const
  {
    return m_WorkThreads.size();
  }

  TCPConnectionPool&
  Loop::connection_pool()
  {
    return *m_ConnectionPool;
  }

}  // namespace llarp::uv
#endif
