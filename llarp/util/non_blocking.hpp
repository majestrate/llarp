#pragma once

#include "fd.hpp"

namespace llarp::util
{
  /// sets nonblocking on a FD.
  struct NonBlocking
  {
    explicit NonBlocking(const FD& fd);
    /// get the flags this FD started with.
    int
    old_flags() const;

    NonBlocking(const NonBlocking&) = delete;
    NonBlocking(NonBlocking&&) = delete;

   private:
    explicit NonBlocking(int fd);
    int m_Flags;
  };

}  // namespace llarp::util
