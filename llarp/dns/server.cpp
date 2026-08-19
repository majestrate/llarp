#include <llarp/util/alloc.h>
#include "server.hpp"
#include <llarp/constants/platform.hpp>
#include "dns.hpp"
#include <iterator>
#include <llarp/crypto/crypto.hpp>
#include <array>
#include <stdexcept>
#include <utility>
#include <llarp/ev/udp_handle.hpp>
#include <optional>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <fmt/core.h>
#include <llarp/util/logging.hpp>
#include "sd_platform.hpp"
#include "nm_platform.hpp"
#include "apple_platform.hpp"

namespace llarp::dns
{
  static auto logcat = log::Cat("dns");

  void
  QueryJob_Base::Cancel()
  {
    Message reply{m_Query};
    reply.AddServFail();
    SendReply(reply.ToBuffer());
  }

  /// sucks up udp packets from a bound socket and feeds it to a server
  class UDPReader : public PacketSource_Base, public std::enable_shared_from_this<UDPReader>
  {
    Server& m_DNS;
    std::shared_ptr<llarp::UDPHandle> m_udp;
    SockAddr m_LocalAddr;

   public:
    explicit UDPReader(Server& dns, const EventLoop_ptr& loop, llarp::SockAddr bindaddr)
        : m_DNS{dns}
    {
      m_udp = loop->make_udp([&](auto&, SockAddr src, llarp::OwnedBuffer buf) {
        if (src == m_LocalAddr)
          return;
        if (not m_DNS.MaybeHandlePacket(shared_from_this(), m_LocalAddr, src, std::move(buf)))
        {
          log::warning(logcat, "did not handle dns packet from {} to {}", src, m_LocalAddr);
        }
      });
      m_udp->listen(bindaddr);
      if (auto maybe_addr = BoundOn())
      {
        m_LocalAddr = *maybe_addr;
      }
      else
        throw std::runtime_error{"cannot find which address our dns socket is bound on"};
    }

    std::optional<SockAddr>
    BoundOn() const override
    {
      return m_udp->LocalAddr();
    }

    bool
    WouldLoop(const SockAddr& to, const SockAddr&) const override
    {
      return to != m_LocalAddr;
    }

    void
    SendTo(const SockAddr& to, const SockAddr&, llarp::OwnedBuffer buf) const override
    {
      m_udp->send(to, std::move(buf));
    }

    void
    Stop() override
    {
      m_udp->close();
    }
  };

  namespace forwarder
  {
    class Resolver;

    class Query : public QueryJob_Base, public std::enable_shared_from_this<Query>
    {
      std::shared_ptr<PacketSource_Base> src;
      SockAddr resolverAddr;
      SockAddr askerAddr;

     public:
      explicit Query(
          std::weak_ptr<Resolver> parent_,
          Message query,
          std::shared_ptr<PacketSource_Base> pktsrc,
          SockAddr toaddr,
          SockAddr fromaddr)
          : QueryJob_Base{std::move(query)}
          , src{std::move(pktsrc)}
          , resolverAddr{std::move(toaddr)}
          , askerAddr{std::move(fromaddr)}
          , parent{parent_}
      {}
      std::weak_ptr<Resolver> parent;

      void
      SendReply(llarp::OwnedBuffer replyBuf) override;
    };

    /// Resolver_Base that forwards DNS queries to upstream servers via UDP and TCP
    class Resolver final : public Resolver_Base, public std::enable_shared_from_this<Resolver>
    {
      std::weak_ptr<EventLoop> m_Loop;
      std::shared_ptr<llarp::UDPHandle> m_upstream_udp;
      std::optional<SockAddr> m_LocalAddr;
      size_t m_next_upstream{0};
      uint16_t m_next_msg_id{0};
      bool m_up{false};

      // Pending queries keyed by DNS message ID
      struct PendingQuery
      {
        std::shared_ptr<Query> query;
        SockAddr upstream;
        llarp::OwnedBuffer raw_query;
        bool tcp_inflight{false};
      };
      std::unordered_map<uint16_t, PendingQuery> m_Pending;

      llarp::DnsConfig m_conf;

      static bool
      send_exact(int fd, const byte_t* data, size_t size)
      {
        size_t sent = 0;
        while (sent < size)
        {
          const auto n = ::send(fd, data + sent, size - sent, 0);
          if (n <= 0)
            return false;
          sent += static_cast<size_t>(n);
        }
        return true;
      }

      static bool
      recv_exact(int fd, byte_t* data, size_t size)
      {
        size_t got = 0;
        while (got < size)
        {
          const auto n = ::recv(fd, data + got, size - got, 0);
          if (n <= 0)
            return false;
          got += static_cast<size_t>(n);
        }
        return true;
      }

      void
      StartTCPFallback(uint16_t msg_id, const PendingQuery& pending)
      {
        auto loop = m_Loop.lock();
        if (not loop)
          return;

        auto response = std::make_shared<std::vector<byte_t>>();
        auto error = std::make_shared<std::string>();

        std::weak_ptr<Query> weak_query = pending.query;
        auto weak_self = weak_from_this();
        auto upstream = pending.upstream;
        auto query_bytes = std::make_shared<std::vector<byte_t>>(pending.raw_query.sz);
        std::copy_n(pending.raw_query.buf.get(), pending.raw_query.sz, query_bytes->data());

        auto work = std::make_unique<EventLoopWork>(
            [weak_self, weak_query, msg_id, upstream, response, error](bool cancelled) {
              if (cancelled)
                return;

              auto self = weak_self.lock();
              if (not self)
                return;

              auto it = self->m_Pending.find(msg_id);
              if (it == self->m_Pending.end() or it->second.query != weak_query.lock())
                return;

              if (not error->empty())
              {
                log::warning(
                    logcat,
                    "dns tcp fallback for id {:#06x} to {} failed: {}",
                    msg_id,
                    upstream,
                    *error);
                auto failed = std::move(it->second.query);
                self->m_Pending.erase(it);
                failed->Cancel();
                return;
              }

              if (response->size() < MessageHeader::Size)
              {
                log::warning(
                    logcat,
                    "dns tcp fallback for id {:#06x} from {} returned truncated response",
                    msg_id,
                    upstream);
                auto failed = std::move(it->second.query);
                self->m_Pending.erase(it);
                failed->Cancel();
                return;
              }

              OwnedBuffer reply{response->size()};
              std::copy_n(response->data(), response->size(), reply.buf.get());
              self->ProcessUpstreamResponse(upstream, std::move(reply), true);
            });

        work->add_work([upstream, query_bytes, response, error]() mutable {
          int fd = -1;
          auto fail = [&](std::string_view err) {
            *error = err;
            if (fd >= 0)
            {
              ::close(fd);
              fd = -1;
            }
          };

          fd = ::socket(upstream.Family(), SOCK_STREAM, IPPROTO_TCP);
          if (fd < 0)
          {
            fail(fmt::format("socket() failed: {}", std::strerror(errno)));
            return;
          }

          timeval timeout{5, 0};
          ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
          ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

          if (::connect(fd, static_cast<const sockaddr*>(upstream), upstream.sockaddr_len()) != 0)
          {
            fail(fmt::format("connect() failed: {}", std::strerror(errno)));
            return;
          }

          if (query_bytes->size() > 0xffff)
          {
            fail("query too large for dns tcp framing");
            return;
          }

          std::vector<byte_t> framed;
          framed.resize(query_bytes->size() + 2);
          framed[0] = static_cast<byte_t>((query_bytes->size() >> 8) & 0xff);
          framed[1] = static_cast<byte_t>(query_bytes->size() & 0xff);
          std::copy_n(query_bytes->data(), query_bytes->size(), framed.data() + 2);

          if (not send_exact(fd, framed.data(), framed.size()))
          {
            fail(fmt::format("send() failed: {}", std::strerror(errno)));
            return;
          }

          byte_t lenbuf[2];
          if (not recv_exact(fd, lenbuf, sizeof(lenbuf)))
          {
            fail(fmt::format("recv(length) failed: {}", std::strerror(errno)));
            return;
          }

          const auto resp_len = static_cast<size_t>((lenbuf[0] << 8) | lenbuf[1]);
          response->resize(resp_len);

          if (not recv_exact(fd, response->data(), resp_len))
          {
            fail(fmt::format("recv(payload) failed: {}", std::strerror(errno)));
            return;
          }

          ::close(fd);
        });

        loop->queue_slow_work(std::move(work));
      }

      void
      ProcessUpstreamResponse(SockAddr from, llarp::OwnedBuffer buf, bool from_tcp = false)
      {
        if (buf.sz < MessageHeader::Size)
        {
          log::warning(logcat, "got truncated dns response from upstream {}", from);
          return;
        }

        // Read the DNS message ID from the response
        llarp_buffer_t hdr_buf{buf};
        MessageHeader hdr;
        if (not hdr.Decode(&hdr_buf))
        {
          log::warning(logcat, "got malformed dns response from upstream {}", from);
          return;
        }

        auto it = m_Pending.find(hdr.id);
        if (it == m_Pending.end())
        {
          log::trace(
              logcat, "got dns response from {} with unknown id {:#06x}, ignoring", from, hdr.id);
          return;
        }

        if (from != it->second.upstream)
        {
          log::warning(
              logcat,
              "got dns response for id {:#06x} from unexpected upstream {} (expected {}), "
              "ignoring",
              hdr.id,
              from,
              it->second.upstream);
          return;
        }

        if (not from_tcp and (hdr.fields & flags_TC))
        {
          if (not it->second.tcp_inflight)
          {
            it->second.tcp_inflight = true;
            log::debug(
                logcat,
                "dns response for id {:#06x} from {} is truncated; retrying via tcp",
                hdr.id,
                from);
            StartTCPFallback(hdr.id, it->second);
          }
          return;
        }

        auto pending = std::move(it->second);
        m_Pending.erase(it);

        log::trace(logcat, "got dns response from upstream {}, forwarding to userland", from);

        // Rewrite the response ID to match the original query ID
        uint16_t orig_id = pending.query->Underlying().hdr_id;
        if (hdr.id != orig_id)
        {
          llarp_buffer_t rewrite_buf{buf};
          MessageHeader rewrite_hdr;
          rewrite_hdr.Decode(&rewrite_buf);
          rewrite_hdr.id = orig_id;
          rewrite_buf.cur = rewrite_buf.base;
          rewrite_hdr.Encode(&rewrite_buf);
        }

        pending.query->SendReply(std::move(buf));
      }

      void
      OnUpstreamResponse(SockAddr from, llarp::OwnedBuffer buf)
      {
        ProcessUpstreamResponse(std::move(from), std::move(buf));
      }

     public:
      explicit Resolver(const EventLoop_ptr& loop, llarp::DnsConfig conf)
          : m_Loop{loop}, m_conf{std::move(conf)}
      {
        Up();
      }

      ~Resolver() override
      {
        Down();
      }

      std::string_view
      ResolverName() const override
      {
        return "forwarder";
      }

      std::optional<SockAddr>
      GetLocalAddr() const override
      {
        return m_LocalAddr;
      }

      void
      RemovePending(const std::shared_ptr<Query>& query)
      {
        for (auto it = m_Pending.begin(); it != m_Pending.end();)
        {
          if (it->second.query == query)
            it = m_Pending.erase(it);
          else
            ++it;
        }
      }

      void
      Up()
      {
        if (m_up)
          throw std::logic_error{"Internal error: attempt to Up() dns server multiple times"};

        if (auto loop = m_Loop.lock())
        {
          m_upstream_udp = loop->make_udp([this](auto&, SockAddr src, llarp::OwnedBuffer buf) {
            OnUpstreamResponse(std::move(src), std::move(buf));
          });

          // Bind to any available port for outgoing queries
          if (m_conf.m_upstreamDNS.empty() or m_conf.m_upstreamDNS.front().isIPv4())
            m_upstream_udp->listen(SockAddr{"0.0.0.0:0"});
          else
            m_upstream_udp->listen(SockAddr{"[::]:0"});
          m_LocalAddr = m_upstream_udp->LocalAddr();

          if (m_LocalAddr)
            log::info(logcat, "dns forwarder sending queries from {}", *m_LocalAddr);
          else
            log::warning(logcat, "dns forwarder could not determine local address");
        }

        m_up = true;
      }

      void
      Down() override
      {
        m_up = false;
        if (m_upstream_udp)
          m_upstream_udp->close();

        if (not m_Pending.empty())
        {
          log::debug(logcat, "cancelling {} pending queries", m_Pending.size());
          auto copy = std::move(m_Pending);
          for (auto& [id, pending] : copy)
            pending.query->Cancel();
        }
      }

      int
      Rank() const override
      {
        return 10;
      }

      void
      ResetResolver(std::optional<std::vector<SockAddr>> replace_upstream) override
      {
        Down();
        if (replace_upstream)
          m_conf.m_upstreamDNS = std::move(*replace_upstream);
        m_next_upstream = 0;
        Up();
      }

      bool
      MaybeHookDNS(
          std::shared_ptr<PacketSource_Base> source,
          const Message& query,
          const SockAddr& to,
          const SockAddr& from) override
      {
        auto tmp = std::make_shared<Query>(weak_from_this(), query, source, to, from);

        if (query.questions.empty())
        {
          log::info(
              logcat,
              "dns from {} to {} has empty query questions, sending failure reply",
              from,
              to);
          tmp->Cancel();
          return true;
        }

        for (const auto& q : query.questions)
        {
          if (q.HasTLD(".loki") or q.HasTLD(".snode"))
          {
            log::warning(
                logcat,
                "dns from {} to {} is for .loki or .snode but got to the forwarder resolver, "
                "sending failure reply",
                from,
                to);
            tmp->Cancel();
            return true;
          }
        }

        if (not m_up or not m_upstream_udp or m_conf.m_upstreamDNS.empty())
        {
          log::debug(
              logcat,
              "dns from {} to {} got to the forwarder resolver, but it isn't set up, "
              "sending failure reply",
              from,
              to);
          tmp->Cancel();
          return true;
        }

        // Pick the next upstream server (round-robin)
        const auto& upstream = m_conf.m_upstreamDNS[m_next_upstream % m_conf.m_upstreamDNS.size()];
        m_next_upstream++;

        // Build raw DNS query packet
        auto raw = query.ToBuffer();

        // Reserve a unique upstream DNS message ID and rewrite if needed
        uint16_t msg_id = query.hdr_id;
        if (m_Pending.count(msg_id))
        {
          bool found = false;
          for (size_t i = 0; i < 65536; ++i)
          {
            const auto candidate = static_cast<uint16_t>(m_next_msg_id++);
            if (not m_Pending.count(candidate))
            {
              msg_id = candidate;
              found = true;
              break;
            }
          }

          if (not found)
          {
            log::warning(logcat, "dns pending table is full, sending failure reply");
            tmp->Cancel();
            return true;
          }

          llarp_buffer_t rewrite_buf{raw};
          MessageHeader rewrite_hdr;
          if (not rewrite_hdr.Decode(&rewrite_buf))
          {
            log::warning(logcat, "failed to decode dns query header for id rewrite");
            tmp->Cancel();
            return true;
          }

          rewrite_hdr.id = msg_id;
          rewrite_buf.cur = rewrite_buf.base;
          if (not rewrite_hdr.Encode(&rewrite_buf))
          {
            log::warning(logcat, "failed to encode dns query header for id rewrite");
            tmp->Cancel();
            return true;
          }
        }

        log::trace(logcat, "dns from {} to {} forwarding to upstream {}", from, to, upstream);

        // Send the raw query to the upstream DNS server
        llarp_buffer_t send_buf{raw};
        if (not m_upstream_udp->send(upstream, send_buf))
        {
          log::warning(logcat, "failed to send dns query to upstream {}", upstream);
          tmp->Cancel();
          return true;
        }

        // Track the pending query
        m_Pending.emplace(msg_id, PendingQuery{tmp, upstream, std::move(raw)});

        if (auto loop = m_Loop.lock())
        {
          auto weak_self = weak_from_this();
          std::weak_ptr<Query> weak_query = tmp;
          loop->call_later(std::chrono::seconds{10}, [weak_self, weak_query, msg_id]() {
            if (auto self = weak_self.lock())
            {
              auto it = self->m_Pending.find(msg_id);
              if (it != self->m_Pending.end() and it->second.query == weak_query.lock())
              {
                log::debug(
                    logcat, "dns query id {:#06x} timed out waiting for upstream reply", msg_id);
                auto timed_out = std::move(it->second.query);
                self->m_Pending.erase(it);
                timed_out->Cancel();
              }
            }
          });
        }

        return true;
      }
    };

    void
    Query::SendReply(llarp::OwnedBuffer replyBuf)
    {
      if (m_Done.test_and_set())
        return;
      auto parent_ptr = parent.lock();
      if (parent_ptr)
      {
        log::trace(
            logcat,
            "forwarding dns response from upstream to userland (resolverAddr: {}, "
            "askerAddr: {})",
            resolverAddr,
            askerAddr);
        src->SendTo(askerAddr, resolverAddr, std::move(replyBuf));
        parent_ptr->RemovePending(shared_from_this());
      }
      else
        log::error(logcat, "no parent");
    }
  }  // namespace forwarder

  Server::Server(EventLoop_ptr loop, llarp::DnsConfig conf, unsigned int netif)
      : m_Loop{std::move(loop)}
      , m_Config{std::move(conf)}
      , m_Platform{CreatePlatform()}
      , m_NetIfIndex{std::move(netif)}
  {}

  std::vector<std::weak_ptr<Resolver_Base>>
  Server::GetAllResolvers() const
  {
    return {m_Resolvers.begin(), m_Resolvers.end()};
  }

  void
  Server::Start()
  {
    // set up udp sockets
    for (const auto& addr : m_Config.m_bind)
    {
      if (auto ptr = MakePacketSourceOn(addr, m_Config))
        AddPacketSource(std::move(ptr));
    }

    // add default resolver as needed
    if (auto ptr = MakeDefaultResolver())
      AddResolver(ptr);
  }

  std::shared_ptr<I_Platform>
  Server::CreatePlatform() const
  {
    auto plat = std::make_shared<Multi_Platform>();
    if constexpr (llarp::platform::has_systemd)
    {
      plat->add_impl(std::make_unique<SD_Platform_t>());
      plat->add_impl(std::make_unique<NM_Platform_t>());
    }
    if constexpr (llarp::platform::is_apple)
      plat->add_impl(std::make_unique<Apple_Platform_t>());
    return plat;
  }

  std::shared_ptr<PacketSource_Base>
  Server::MakePacketSourceOn(const llarp::SockAddr& addr, const llarp::DnsConfig&)
  {
    return std::make_shared<UDPReader>(*this, m_Loop, addr);
  }

  std::shared_ptr<Resolver_Base>
  Server::MakeDefaultResolver()
  {
    if (m_Config.m_upstreamDNS.empty())
    {
      log::info(
          logcat,
          "explicitly no upstream dns providers specified, we will not resolve anything but .loki "
          "and .snode");
      return nullptr;
    }

    return std::make_shared<forwarder::Resolver>(m_Loop, m_Config);
  }

  std::vector<SockAddr>
  Server::BoundPacketSourceAddrs() const
  {
    std::vector<SockAddr> addrs;
    for (const auto& src : m_PacketSources)
    {
      if (auto ptr = src.lock())
        if (auto maybe_addr = ptr->BoundOn())
          addrs.emplace_back(*maybe_addr);
    }
    return addrs;
  }

  std::optional<SockAddr>
  Server::FirstBoundPacketSourceAddr() const
  {
    for (const auto& src : m_PacketSources)
    {
      if (auto ptr = src.lock())
        if (auto bound = ptr->BoundOn())
          return bound;
    }
    return std::nullopt;
  }

  void
  Server::AddResolver(std::weak_ptr<Resolver_Base> resolver)
  {
    m_Resolvers.insert(resolver);
  }

  void
  Server::AddResolver(std::shared_ptr<Resolver_Base> resolver)
  {
    m_OwnedResolvers.insert(resolver);
    AddResolver(std::weak_ptr<Resolver_Base>{resolver});
  }

  void
  Server::AddPacketSource(std::weak_ptr<PacketSource_Base> pkt)
  {
    m_PacketSources.push_back(pkt);
  }

  void
  Server::AddPacketSource(std::shared_ptr<PacketSource_Base> pkt)
  {
    AddPacketSource(std::weak_ptr<PacketSource_Base>{pkt});
    m_OwnedPacketSources.push_back(std::move(pkt));
  }

  void
  Server::Stop()
  {
    for (const auto& resolver : m_Resolvers)
    {
      if (auto ptr = resolver.lock())
        ptr->Down();
    }
  }

  void
  Server::Reset()
  {
    for (const auto& resolver : m_Resolvers)
    {
      if (auto ptr = resolver.lock())
        ptr->ResetResolver();
    }
  }

  void
  Server::SetDNSMode(bool all_queries)
  {
    if (auto maybe_addr = FirstBoundPacketSourceAddr())
      m_Platform->set_resolver(m_NetIfIndex, *maybe_addr, all_queries);
  }

  bool
  Server::MaybeHandlePacket(
      std::shared_ptr<PacketSource_Base> ptr,
      const SockAddr& to,
      const SockAddr& from,
      llarp::OwnedBuffer buf)
  {
    // dont process to prevent feedback loop
    if (ptr->WouldLoop(to, from))
    {
      log::warning(logcat, "preventing dns packet replay to={} from={}", to, from);
      return false;
    }

    auto maybe = MaybeParseDNSMessage(buf);
    if (not maybe)
    {
      log::warning(logcat, "invalid dns message format from {} to dns listener on {}", from, to);
      return false;
    }

    auto& msg = *maybe;
    // we don't provide a DoH resolver because it requires verified TLS
    // TLS needs X509/ASN.1-DER and opting into the Root CA Cabal
    // thankfully mozilla added a backdoor that allows ISPs to turn it off
    // so we disable DoH for firefox using mozilla's ISP backdoor
    // see: https://github.com/oxen-io/lokinet/issues/832
    for (const auto& q : msg.questions)
    {
      // is this firefox looking for their backdoor record?
      if (q.IsName("use-application-dns.net"))
      {
        // yea it is, let's turn off DoH because god is dead.
        msg.AddNXReply();
        // press F to pay respects and send it back where it came from
        ptr->SendTo(from, to, msg.ToBuffer());
        return true;
      }
    }

    for (const auto& resolver : m_Resolvers)
    {
      if (auto res_ptr = resolver.lock())
      {
        log::trace(
            logcat, "check resolver {} for dns from {} to {}", res_ptr->ResolverName(), from, to);
        if (res_ptr->MaybeHookDNS(ptr, msg, to, from))
        {
          log::trace(
              logcat, "resolver {} handling dns from {} to {}", res_ptr->ResolverName(), from, to);
          return true;
        }
      }
    }
    return false;
  }

}  // namespace llarp::dns
