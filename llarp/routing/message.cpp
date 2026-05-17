#include "message.hpp"

namespace llarp::routing
{
  size_t
  IMessage::total_size() const noexcept
  {
    size_t sz{};
    std::array<uint8_t, constants::routing_message_max_size> buffer;
    llarp_buffer_t buf{buffer};
    if (BEncode(&buf))
      sz = std::distance(buf.base, buf.cur);
    return sz;
  }

  size_t
  IMessage::overhead() const noexcept
  {
    return overhead_for(from) + overhead_for(version) + overhead_for(S);
  }
}  // namespace llarp::routing