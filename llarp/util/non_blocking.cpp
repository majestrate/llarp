#include "non_blocking.hpp"
#include <stdexcept>
#include <fmt/format.h>
#include <fcntl.h>

namespace llarp::util
{

  NonBlocking::NonBlocking(int fd)
  {
    m_Flags = fcntl(fd, F_GETFL, 0);
    if (m_Flags == -1)
      throw std::runtime_error{fmt::format("fcntl(F_GETFL): {}", strerror(errno))};

    auto flags = m_Flags | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1)
      throw std::runtime_error{fmt::format("fcntl(F_SETFL): {}", strerror(errno))};
  }

  NonBlocking::NonBlocking(const FD& fd) : NonBlocking{fd.fd()}
  {}

  int
  NonBlocking::old_flags() const
  {
    return m_Flags;
  }
}  // namespace llarp::util
