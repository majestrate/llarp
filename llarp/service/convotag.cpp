#include <llarp/util/alloc.h>
#include "convotag.hpp"
#include <llarp/net/ip.hpp>

namespace llarp::service
{

  sockaddr_in6
  ConvoTag::ToV6() const
  {
    sockaddr_in6 saddr{};
    saddr.sin6_family = AF_INET6;
    std::copy_n(data(), size(), saddr.sin6_addr.s6_addr);
    saddr.sin6_addr.s6_addr[0] = 0xfc;
    return saddr;
  }

  void
  ConvoTag::FromV6(sockaddr_in6 saddr)
  {
    std::copy_n(saddr.sin6_addr.s6_addr, size(), data());
  }

}  // namespace llarp::service
