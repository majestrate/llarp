#pragma once
#include "ip_range.hpp"

namespace llarp
{

  static inline std::array bogonRanges_v6 = {
      // zero
      IPRange{ToNet(huint128_t{0}), net::netmask_ipv6_bits(128)},
      // loopback
      IPRange{ToNet(huint128_t{1}), net::netmask_ipv6_bits(128)},
      // yggdrasil
      IPRange{
          ToNet(huint128_t{uint128_t{0x0200'0000'0000'0000UL, 0UL}}), net::netmask_ipv6_bits(7)},
      // multicast
      IPRange{
          ToNet(huint128_t{uint128_t{0xff00'0000'0000'0000UL, 0UL}}), net::netmask_ipv6_bits(8)},
      // local
      IPRange{
          ToNet(huint128_t{uint128_t{0xfc00'0000'0000'0000UL, 0UL}}), net::netmask_ipv6_bits(8)},
      // local
      IPRange{
          ToNet(huint128_t{uint128_t{0xf800'0000'0000'0000UL, 0UL}}), net::netmask_ipv6_bits(8)}};

  static inline std::array bogonRanges_v4 = {
      IPRange::FromIPv4(0, 0, 0, 0, 8),
      IPRange::FromIPv4(10, 0, 0, 0, 8),
      IPRange::FromIPv4(100, 64, 0, 0, 10),
      IPRange::FromIPv4(127, 0, 0, 0, 8),
      IPRange::FromIPv4(169, 254, 0, 0, 16),
      IPRange::FromIPv4(172, 16, 0, 0, 12),
      IPRange::FromIPv4(192, 0, 0, 0, 24),
      IPRange::FromIPv4(192, 0, 2, 0, 24),
      IPRange::FromIPv4(192, 88, 99, 0, 24),
      IPRange::FromIPv4(192, 168, 0, 0, 16),
      IPRange::FromIPv4(198, 18, 0, 0, 15),
      IPRange::FromIPv4(198, 51, 100, 0, 24),
      IPRange::FromIPv4(203, 0, 113, 0, 24),
      IPRange::FromIPv4(224, 0, 0, 0, 4),
      IPRange::FromIPv4(240, 0, 0, 0, 4)};
}  // namespace llarp
