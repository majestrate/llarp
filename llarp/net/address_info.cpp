#include "address_info.hpp"
#include <stdexcept>

#include <arpa/inet.h>
#include "llarp/net/sock_addr.hpp"
#include "net.hpp"
#include <llarp/util/bencode.h>
#include <llarp/util/mem.h>
#include <netinet/in.h>

#include <cstring>

namespace llarp
{
  bool
  operator==(const AddressInfo& lhs, const AddressInfo& rhs)
  {
    // we don't care about rank
    return lhs.pubkey == rhs.pubkey && lhs.port == rhs.port && lhs.dialect == rhs.dialect
        && lhs.ip == rhs.ip;
  }

  bool
  operator<(const AddressInfo& lhs, const AddressInfo& rhs)
  {
    const auto l_ip = ToHost(lhs.ip);
    const auto r_ip = ToHost(rhs.ip);
    return std::tie(lhs.rank, l_ip, lhs.port) < std::tie(rhs.rank, r_ip, rhs.port);
  }

  std::variant<nuint32_t, nuint128_t>
  AddressInfo::IP() const
  {
    return SockAddr{ip}.getIP();
  }

  bool
  AddressInfo::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    uint64_t i;
    std::array<char, 128> tmp{};

    llarp_buffer_t strbuf;

    // rank
    if (key.startswith("c"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      rank = i;
      return true;
    }

    // dialect
    if (key.startswith("d"))
    {
      if (!bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz > tmp.size())
        return false;
      std::copy_n(strbuf.base, strbuf.sz, tmp.data());
      tmp[strbuf.sz] = 0;
      dialect = std::string(tmp.data());
      return true;
    }

    // encryption public key
    if (key.startswith("e"))
    {
      return pubkey.BDecode(buf);
    }

    // ip address
    if (key.startswith("i"))
    {
      if (!bencode_read_string(buf, &strbuf))
        return false;
      if (strbuf.sz >= sizeof(tmp))
        return false;
      std::copy_n(strbuf.base, strbuf.sz, tmp.data());
      tmp[strbuf.sz] = 0;
      return inet_pton(AF_INET6, tmp.data(), reinterpret_cast<uint8_t*>(&ip.n)) == 1;
    }

    // port
    if (key.startswith("p"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;

      if (i > 65536 || i <= 0)
        return false;

      port = i;
      return true;
    }

    // version
    if (key.startswith("v"))
    {
      if (!bencode_read_integer(buf, &i))
        return false;
      return i == llarp::constants::proto_version;
    }

    // bad key
    return false;
  }

  bool
  AddressInfo::BEncode(llarp_buffer_t* buff) const
  {
    std::array<char, 128> ipbuff{};
    const char* ipstr;
    if (!bencode_start_dict(buff))
      return false;
    /* rank */
    if (!bencode_write_bytestring(buff, "c", 1))
      return false;
    if (!bencode_write_uint64(buff, rank))
      return false;
    /* dialect */
    if (!bencode_write_bytestring(buff, "d", 1))
      return false;
    if (!bencode_write_bytestring(buff, dialect.c_str(), dialect.size()))
      return false;
    /* encryption key */
    if (!bencode_write_bytestring(buff, "e", 1))
      return false;
    if (!bencode_write_bytestring(buff, pubkey.data(), PUBKEYSIZE))
      return false;
    /** ip */
    ipstr =
        inet_ntop(AF_INET6, reinterpret_cast<const uint8_t*>(&ip.n), ipbuff.data(), ipbuff.size());
    if (!ipstr)
      return false;
    if (!bencode_write_bytestring(buff, "i", 1))
      return false;
    if (!bencode_write_bytestring(buff, ipstr, strnlen(ipstr, ipbuff.size())))
      return false;
    /** port */
    if (!bencode_write_bytestring(buff, "p", 1))
      return false;
    if (!bencode_write_uint64(buff, port))
      return false;

    /** version */
    if (!bencode_write_uint64_entry(buff, "v", 1, llarp::constants::proto_version))
      return false;
    /** end */
    return bencode_end(buff);
  }

  void
  AddressInfo::fromSockAddr(const SockAddr& addr)
  {
    const auto* addr6 = static_cast<const sockaddr_in6*>(addr);
    std::copy_n(addr6->sin6_addr.s6_addr, 16, reinterpret_cast<uint8_t*>(&ip.n));
    port = addr.getPort();
  }

  std::string
  AddressInfo::ToString() const
  {
    std::array<char, INET6_ADDRSTRLEN> tmp{};
    inet_ntop(AF_INET6, reinterpret_cast<const uint8_t*>(&ip.n), tmp.data(), tmp.size());
    return fmt::format("[{}]:{}", std::string_view{tmp.data()}, port);
  }
}  // namespace llarp
