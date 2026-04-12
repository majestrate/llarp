#include <llarp/util/alloc.h>
#include "ev.hpp"

#include "tcp_handle.hpp"

#include <llarp/util/mem.hpp>
#include <llarp/util/str.hpp>
#include <cstddef>
#include <cstring>
#include <string_view>

#ifdef USE_IO_URING
#include "io_uring.hpp"
#else
#include "libuv.hpp"
#endif
#include <llarp/net/net.hpp>

namespace llarp
{
  static auto logcat = log::Cat("evloop");

  EventLoop_ptr
  EventLoop::create(size_t threads, size_t queueLength)
  {
#ifdef USE_IO_URING
    return std::make_shared<llarp::io_uring::Loop>(queueLength, threads);
#else
    return std::make_shared<llarp::uv::Loop>(queueLength, threads);
#endif
  }

  const net::Platform*
  EventLoop::Net_ptr() const
  {
    return net::Platform::Default_ptr();
  }

  EventLoopWork::EventLoopWork(std::function<void(bool)> cleanup) : _cleanup{std::move(cleanup)}
  {}

  void
  EventLoopWork::work() const
  {
    for (const auto& work : _pure_work)
      work();
    log::debug(logcat, "work done");
  }

  void
  EventLoopWork::cleanup(bool cancel) const
  {
    if (_cleanup)
      _cleanup(cancel);
  }

  void
  TCPConnection::SetRecvHandler(RecvHandler h)
  {
    m_RecvHandler = h;
  }

  void
  TCPConnection::Untrack()
  {
    m_Pool.RemoveConn(shared_from_this());
  }

  void
  TCPAcceptor::Untrack()
  {
    m_Pool.RemoveConn(shared_from_this());
  }

  std::shared_ptr<TCPConnection>
  TCPAcceptor::MakeConn()
  {
    return m_Pool.MakeConnection(std::nullopt, [](std::optional<OwnedBuffer>, std::error_code) {});
  }

  void
  TCPConnectionPool::RemoveConn(std::shared_ptr<TCPConnection> conn)
  {
    auto laddr = conn->LocalAddr();
    auto raddr = conn->RemoteAddr();
    std::erase_if(m_Connections, [laddr, raddr](const auto& conn) {
      return laddr == conn->LocalAddr() and raddr == conn->RemoteAddr();
    });
  }

  void
  TCPConnectionPool::RemoveConn(std::shared_ptr<TCPAcceptor> acceptor)
  {
    auto maybe_laddr = acceptor->LocalAddr();
    if (not maybe_laddr)
      return;
    const auto& laddr = *maybe_laddr;
    std::erase_if(m_Connections, [laddr](const auto& conn) {
      return laddr == conn->LocalAddr();
    });
  }

  TCPConnection::TCPConnection(RecvHandler handler, TCPConnectionPool& pool)
      : m_RecvHandler{handler}, m_Pool{pool}
  {}

  TCPAcceptor::TCPAcceptor(AcceptHandler handler, TCPConnectionPool& pool)
      : m_AcceptHandler{handler}, m_Pool{pool}
  {}
}  // namespace llarp
