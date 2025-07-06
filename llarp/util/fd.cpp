#include <llarp/util/alloc.h>
#include "fd.hpp"
#include <unistd.h>

namespace llarp::util
{
  FD::FD(int fd) : m_FD{fd}
  {}

  FD::~FD()
  {
    if (m_FD != -1)
      ::close(m_FD);
  }
}  // namespace llarp::util
