#pragma once
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cerrno>
#include <stdexcept>
#include <fmt/format.h>

#include <llarp/util/ioctl.hpp>

namespace llarp::vpn
{
  class permission_error : public std::runtime_error
  {
   public:
    using std::runtime_error::runtime_error;
  };

  struct IOCTL : util::IOCTL
  {
    explicit IOCTL(int af) : util::IOCTL{::socket(af, SOCK_DGRAM, IPPROTO_IP)} {};
  };
}  // namespace llarp::vpn
