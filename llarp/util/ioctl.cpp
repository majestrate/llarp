#include <llarp/util/alloc.h>
#include "ioctl.hpp"

namespace llarp::util
{

  IOCTL::IOCTL(int fd) : m_own_FD{fd}, m_FD{m_own_FD}
  {
    if (m_FD.fd() == -1)
      throw std::invalid_argument{strerror(errno)};
  }

  IOCTL::IOCTL(const FD& fd) : m_own_FD{-1}, m_FD{fd}
  {}
}  // namespace llarp::util
