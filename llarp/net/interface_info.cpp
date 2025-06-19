#include "interface_info.hpp"
#include <fmt/ranges.h>
namespace llarp::net
{
  std::string
  InterfaceInfo::ToString() const
  {
    return fmt::format("{}[idx={}; addrs={};]", name, index, fmt::join(addrs, ","));
  }
}  // namespace llarp::net
