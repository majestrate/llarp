#include "traffic_policy.hpp"
#include "llarp/util/str.hpp"

namespace llarp::net
{
  ProtocolInfo::ProtocolInfo(std::string_view data)
  {
    const auto parts = split(data, "/");
    protocol = get_ip_proto_by_name(std::string{parts[0]});
    if (parts.size() == 2)
    {
      huint16_t portHost{};
      std::string portStr{parts[1]};
      std::string protoName = ip_proto_to_string(protocol);
      if (const auto* serv = ::getservbyname(portStr.c_str(), protoName.c_str()))
      {
        portHost.h = serv->s_port;
      }
      else if (const auto portInt = std::stoi(portStr); portInt > 0)
      {
        portHost.h = portInt;
      }
      else
        throw std::invalid_argument{"invalid port in protocol info: " + portStr};
      port = ToNet(portHost);
    }
    else
      port = std::nullopt;
  }

  bool
  ProtocolInfo::MatchesPacket(const IPPacket& pkt) const
  {
    if (pkt.Header()->protocol != static_cast<std::underlying_type_t<IPProtocol>>(protocol))
      return false;

    if (not port)
      return true;
    if (const auto maybe = pkt.DstPort())
    {
      return *port == *maybe;
    }
    // we can't tell what the port is but the protocol matches and that's good enough
    return true;
  }

  bool
  TrafficPolicy::AllowsTraffic(const IPPacket& pkt) const
  {
    if (protocols.empty() and ranges.empty())
      return true;

    for (const auto& proto : protocols)
    {
      if (proto.MatchesPacket(pkt))
        return true;
    }
    for (const auto& range : ranges)
    {
      huint128_t dst;
      if (pkt.IsV6())
        dst = pkt.dstv6();
      else if (pkt.IsV4())
        dst = pkt.dst4to6();
      else
        return false;
      if (range.Contains(dst))
        return true;
    }
    return false;
  }

  bool
  ProtocolInfo::BDecode(llarp_buffer_t* buf)
  {
    port = std::nullopt;
    std::vector<uint64_t> vals;
    if (not bencode_read_list(
            [&vals](llarp_buffer_t* buf, bool more) {
              if (more)
              {
                uint64_t intval;
                if (not bencode_read_integer(buf, &intval))
                  return false;
                vals.push_back(intval);
              }
              return true;
            },
            buf))
      return false;
    if (vals.empty())
      return false;
    if (vals.size() >= 1)
    {
      if (vals[0] > 255)
        return false;
      protocol = static_cast<IPProtocol>(vals[0]);
    }
    if (vals.size() >= 2)
    {
      if (vals[1] > 65536)
        return false;
      port = ToNet(huint16_t{static_cast<uint16_t>(vals[1])});
    }
    return true;
  }

  bool
  ProtocolInfo::BEncode(llarp_buffer_t* buf) const
  {
    if (not bencode_start_list(buf))
      return false;
    if (not bencode_write_uint64(buf, static_cast<std::underlying_type_t<IPProtocol>>(protocol)))
      return false;
    if (port)
    {
      const auto hostint = ToHost(*port);
      if (not bencode_write_uint64(buf, hostint.h))
        return false;
    }
    return bencode_end(buf);
  }

  bool
  TrafficPolicy::BEncode(llarp_buffer_t* buf) const
  {
    if (not bencode_start_dict(buf))
      return false;

    if (not bencode_write_bytestring(buf, "p", 1))
      return false;

    if (not bencode_start_list(buf))
      return false;

    for (const auto& item : protocols)
    {
      if (not item.BEncode(buf))
        return false;
    }

    if (not bencode_end(buf))
      return false;

    if (not bencode_write_bytestring(buf, "r", 1))
      return false;

    if (not bencode_start_list(buf))
      return false;

    for (const auto& item : ranges)
    {
      if (not item.BEncode(buf))
        return false;
    }

    if (not bencode_end(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  TrafficPolicy::BDecode(llarp_buffer_t* buf)
  {
    return bencode_read_dict(
        [&](llarp_buffer_t* buffer, llarp_buffer_t* key) -> bool {
          if (key == nullptr)
            return true;
          if (key->startswith("p"))
          {
            return BEncodeReadSet(protocols, buffer);
          }
          if (key->startswith("r"))
          {
            return BEncodeReadSet(ranges, buffer);
          }
          return bencode_discard(buffer);
        },
        buf);
  }




}  // namespace llarp::net
