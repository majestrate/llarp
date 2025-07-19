#include <llarp/util/alloc.h>
#include "ev.hpp"
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
  }

  void
  EventLoopWork::cleanup(bool cancel) const
  {
    if (_cleanup)
      _cleanup(cancel);
  }

}  // namespace llarp
