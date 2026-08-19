#include "apple_platform.hpp"

#ifdef __APPLE__

namespace llarp::dns::apple
{
  void
  Platform::set_resolver(unsigned int, llarp::SockAddr, bool)
  {
    // TODO: implement macOS DNS resolver configuration
  }
}  // namespace llarp::dns::apple

#endif  // __APPLE__
