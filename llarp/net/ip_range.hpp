#pragma once

#include "ip.hpp"
#include "net_int.hpp"
#include "net_bits.hpp"
#include <llarp/util/bits.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/types.hpp>

#include <set>
#include <optional>
#include <stdexcept>
#include <string>

namespace llarp
{
  struct IPRange
  {
    using Addr_t = net::ipv6addr_t;
    Addr_t addr = {0};
    Addr_t netmask_bits = {0};

    constexpr IPRange() = default;

    constexpr IPRange(Addr_t address, Addr_t netmask)
        : addr{std::move(address)}, netmask_bits{std::move(netmask)}
    {}

    explicit IPRange(std::string _range)
    {
      if (not FromString(_range))
        throw std::invalid_argument{
            fmt::format("IP string '{}' cannot be parsed as IP range", _range)};
    }

    static inline IPRange
    V4MappedRange()
    {
      return IPRange{Addr_t::from_host(0x0000'ffff'0000'0000UL), net::netmask_ipv6_bits(96)};
    }

    static inline IPRange
    FromIPv4(byte_t a, byte_t b, byte_t c, byte_t d, byte_t mask)
    {
      return IPRange{
          net::ExpandV4(net::ipaddr_ipv4_bits(a, b, c, d)), net::netmask_ipv6_bits(mask + 96)};
    }

    static inline IPRange
    FromIPv4(net::ipv4addr_t addr, net::ipv4addr_t netmask)
    {
      return IPRange{net::ExpandV4(addr), net::netmask_ipv6_bits(bits::count_bits(netmask) + 96)};
    }

    /// return true if this iprange is in the IPv4 mapping range for containing ipv4 addresses
    inline bool
    IsV4() const
    {
      return V4MappedRange().Contains(addr);
    }

    /// get address family
    inline int
    Family() const
    {
      if (IsV4())
        return AF_INET;
      return AF_INET6;
    }

    /// return the number of bits set in the hostmask
    inline int
    HostmaskBits() const
    {
      if (IsV4())
      {
        return bits::count_bits(net::TruncateV6(netmask_bits));
      }
      return bits::count_bits(netmask_bits);
    }

    /// return true if our range and other intersect
    inline bool
    operator*(const IPRange& other) const
    {
      return Contains(other) or other.Contains(*this);
    }

    /// return true if the other range is inside our range
    inline bool
    Contains(const IPRange& other) const
    {
      return Contains(other.addr) and Contains(other.HighestAddr());
    }

    /// return true if ip is contained in this ip range
    constexpr bool
    Contains(const Addr_t& ip) const
    {
      return (addr & netmask_bits) == (ip & netmask_bits);
    }

    /// return true if we are a ipv4 range and contains this ip
    inline bool
    Contains(const net::ipv4addr_t& ip) const
    {
      if (not IsV4())
        return false;
      return Contains(net::ExpandV4(ip));
    }

    inline net::ipaddr_t
    BaseAddr() const
    {
      if (IsV4())
      {
        return ToNet(ToHost(net::TruncateV6(addr)) + huint32_t{1});
      }
      return ToNet(ToHost(addr) + huint128_t{1});
    }

    inline bool
    Contains(const net::ipaddr_t& ip) const
    {
      return var::visit([this](auto&& ip) { return Contains(ip); }, ip);
    }

    /// get the highest address on this range
    inline Addr_t
    HighestAddr() const
    {
      return ToNet(
          ToHost(addr & netmask_bits)
          + (huint128_t{1} << (128 - bits::count_bits_128(netmask_bits.n))) - huint128_t{1});
    }

    inline bool
    operator<(const IPRange& other) const
    {
      const auto maskedA = ToHost(addr & netmask_bits),
                 maskedB = ToHost(other.addr & other.netmask_bits);
      const auto h_net_bits = ToHost(netmask_bits);
      const auto h_other_bits = ToHost(other.netmask_bits);
      return std::tie(maskedA, h_net_bits) < std::tie(maskedB, h_other_bits);
    }

    inline bool
    operator==(const IPRange& other) const
    {
      return addr == other.addr and netmask_bits == other.netmask_bits;
    }

    inline std::string
    ToString() const
    {
      return BaseAddressString() + "/" + std::to_string(HostmaskBits());
    }

    std::string
    BaseAddressString() const;

    std::string
    NetmaskString() const;

    bool
    FromString(std::string str);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);

    /// Finds a free private use range not overlapping the given ranges.
    static std::optional<IPRange>
    FindPrivateRange(const std::set<IPRange>& excluding);
  };

  template <>
  constexpr inline bool IsToStringFormattable<IPRange> = true;

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::IPRange>
  {
    size_t
    operator()(const llarp::IPRange& range) const
    {
      const auto str = range.ToString();
      return std::hash<std::string>{}(str);
    }
  };
}  // namespace std
