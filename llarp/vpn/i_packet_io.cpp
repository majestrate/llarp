#include <llarp/util/alloc.h>
#include "i_packet_io.hpp"

namespace llarp::vpn
{
  void
  I_Packet_IO::Stop()
  {
    if (on_stop)
      on_stop();
  }
}  // namespace llarp::vpn
