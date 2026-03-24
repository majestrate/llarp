#pragma once

#include <llarp/dht/key.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/aligned.hpp>

#include <functional>
#include <numeric>
#include <string>
#include <string_view>
#include <variant>
#include <set>
#include <optional>

namespace llarp::service
{
  /// HS Address
  struct Address
  {
    ALIGNED_BUFFER_MEMBERS(Address, 32)
   public:
    /// if parsed using FromString this contains the subdomain
    /// this member is not used when comparing it's extra data for dns
    std::string subdomain;

    /// list of whitelisted gtld to permit
    static const std::set<std::string> AllowedTLDs;

    /// return true if we permit using this tld
    /// otherwise return false
    static bool
    PermitTLD(const char* tld);

    std::string
    ToString(const char* tld = ".loki") const;

    bool
    FromString(std::string_view str, const char* tld = ".loki");

    explicit Address(const std::string& str)
    {
      if (not FromString(str))
        throw std::runtime_error("invalid address");
    }

    explicit Address(const Data& buf) : m_data{buf}
    {}

    Address(const Address& other) : m_data{other.as_array()}, subdomain{other.subdomain}
    {}

    auto
    operator<=>(const Address& other) const
    {
      return as_array() <=> other.as_array();
    }

    Address&
    operator=(const Address& other) = default;

    dht::Key_t
    ToKey() const;

    RouterID
    ToRouter() const
    {
      return RouterID(data());
    }
  };

  std::optional<std::variant<Address, RouterID>>
  ParseAddress(std::string_view lokinet_addr);

}  // namespace llarp::service
namespace llarp
{
  template <>
  constexpr inline bool IsToStringFormattable<service::Address> = true;

  template <>
  constexpr inline bool is_aligned_buffer<service::Address> = true;

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::service::Address>
  {
    size_t
    operator()(const llarp::service::Address& addr) const
    {
      return std::accumulate(addr.begin(), addr.end(), 0, std::bit_xor{});
    }
  };
}  // namespace std
