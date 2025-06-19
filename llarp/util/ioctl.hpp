#pragma once

#include "fd.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace llarp::util
{
  class permission_error : public std::runtime_error
  {
   public:
    using std::runtime_error::runtime_error;
  };

  struct IOCTL
  {
    explicit IOCTL(const FD& fd);

    IOCTL(const IOCTL&) = delete;
    IOCTL(IOCTL&&) = delete;

    template <typename Command, typename... Args>
    void
    ioctl(Command cmd, Args&&... args)
    {
      if (::ioctl(m_FD.fd(), cmd, std::forward<Args>(args)...) == -1)
      {
        if (errno == EACCES)
        {
          throw permission_error{"we are not allowed to call this ioctl"};
        }
        else
          throw std::runtime_error("ioctl failed: " + std::string{strerror(errno)});
      }
    }

   protected:
    explicit IOCTL(int fd);

    const util::FD m_own_FD;
    const util::FD& m_FD;
  };
}  // namespace llarp::util
