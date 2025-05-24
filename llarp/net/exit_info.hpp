#pragma once

#include <llarp/crypto/types.hpp>
#include <llarp/util/bencode.hpp>

#include <iosfwd>
#include <llarp/net/net_int.hpp>

/**
 * exit_info.h
 *
 * utilities for handling exits on the llarp network
 */

/// Exit info model
namespace llarp
{
  /// deprecated don't use me , this is only for backwards compat
  struct ExitInfo
  {
    in6_addr ipAddress;
    in6_addr netmask;
    PubKey pubkey;
    uint64_t version = llarp::constants::proto_version;

    ExitInfo() = default;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    std::string
    ToString() const;
  };

  template <>
  constexpr inline bool IsToStringFormattable<ExitInfo> = true;

}  // namespace llarp
