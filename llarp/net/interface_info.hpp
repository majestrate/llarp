#pragma once

#include <optional>
#include <string>
#include <vector>
#include <llarp/util/formattable.hpp>
#include "ip_range.hpp"

namespace llarp::net
{
  /// info about a network interface lokinet does not own
  struct InterfaceInfo
  {
    /// human readable name of interface
    std::string name;
    /// interface's index
    unsigned int index;
    /// the addresses owned by this interface
    std::vector<IPRange> addrs;

    std::string
    ToString() const;
  };
}  // namespace llarp::net

template <>
inline constexpr bool llarp::IsToStringFormattable<llarp::net::InterfaceInfo> = true;
