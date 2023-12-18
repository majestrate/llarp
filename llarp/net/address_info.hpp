#pragma once

#include <llarp/crypto/types.hpp>

#include "net.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/mem.h>

#include <string>
#include <vector>

#include <oxenc/variant.h>

/**
 * address_info.hpp
 *
 * utilities for handling addresses on the llarp network
 */

/// address information model
namespace llarp
{
  struct AddressInfo
  {
    uint16_t rank;
    std::string dialect;
    llarp::PubKey pubkey;
    in6_addr ip = {};
    uint16_t port;
    uint64_t version = llarp::constants::proto_version;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf);

    /// get this as an explicit v4 or explicit v6
    net::ipaddr_t
    IP() const;

    /// get this as an v4 or throw if it is not one
    inline net::ipv4addr_t
    IPv4() const
    {
      auto ip = IP();
      if (auto* ptr = std::get_if<net::ipv4addr_t>(&ip))
        return *ptr;
      throw std::runtime_error{"no ipv4 address found in address info"};
    }

    std::string
    ToString() const;
  };


  bool
  operator==(const AddressInfo& lhs, const AddressInfo& rhs);

  bool
  operator<(const AddressInfo& lhs, const AddressInfo& rhs);

  template <>
  constexpr inline bool IsToStringFormattable<AddressInfo> = true;

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::AddressInfo>
  {
    size_t
    operator()(const llarp::AddressInfo& addr) const
    {
      return std::hash<llarp::PubKey>{}(addr.pubkey);
    }
  };
}  // namespace std
