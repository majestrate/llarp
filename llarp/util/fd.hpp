#pragma once

namespace llarp::util
{

  /// RAII wrapper for auto-closing a file descriptor.
  struct FD
  {
    explicit FD(int fd);

    FD(const FD&) = delete;
    FD(FD&&) = delete;

    ~FD();

    inline explicit
    operator int() const
    {
      return fd();
    }

    inline int
    fd() const
    {
      return m_FD;
    }

   private:
    int m_FD;
  };
}  // namespace llarp::util
