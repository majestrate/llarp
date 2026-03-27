#pragma once

#include <cstddef>
#include <llarp/crypto/constants.hpp>

namespace llarp::constants
{
  /// current network wide protocol version
  // TODO: enum class
  constexpr auto proto_version = 0;

  constexpr size_t encrypted_frame_overhead_size = PUBKEYSIZE + TUNNONCESIZE + SHORTHASHSIZE;
  constexpr size_t service_proto_message_max_size = 2048;
  constexpr size_t service_proto_frame_max_size = service_proto_message_max_size * 2;
  constexpr size_t service_proto_message_overhead = 128 + 24 + encrypted_frame_overhead_size;

}  // namespace llarp::constants
