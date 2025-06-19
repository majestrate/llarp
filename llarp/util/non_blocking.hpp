#pragma once

#include "fd.hpp"

namespace llarp::util
{
  /// sets nonblocking on a FD.
  struct NonBlocking
  {
    explicit NonBlocking(int fd);
    explicit NonBlocking(const FD& fd);
    /// get the flags this FD started with.
    int
    old_flags() const;

   private:
    int m_Flags;
  };

}  // namespace llarp::util
