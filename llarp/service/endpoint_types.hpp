#pragma once

#include "pendingbuffer.hpp"
#include "router_lookup_job.hpp"
#include "session.hpp"
#include <llarp/util/compare_ptr.hpp>
#include <llarp/util/thread/queue.hpp>

#include <deque>
#include <memory>
#include <queue>
#include <unordered_map>

namespace llarp
{
  // clang-format off
  namespace exit { struct BaseSession; }
  namespace path { struct Path; using Path_ptr = std::shared_ptr< Path >; }
  namespace routing { struct PathTransferMessage; }
  // clang-format on

  namespace service
  {
    struct IServiceLookup;
    struct OutboundContext;

    using Msg_ptr = std::shared_ptr<routing::PathTransferMessage>;

    using SendEvent_t = std::pair<Msg_ptr, path::Path_ptr>;

    bool
    operator>(const SendEvent_t& lhs, const SendEvent_t& rhs) noexcept;

    using SendMessageQueue_t = thread::Queue<SendEvent_t>;

    using PendingBufferQueue = std::deque<PendingBuffer>;
    using PendingTraffic = std::unordered_map<Address, PendingBufferQueue>;

    using ProtocolMessagePtr = std::shared_ptr<ProtocolMessage>;
    using RecvPacketQueue_t = thread::Queue<ProtocolMessagePtr>;

    using PendingRouters = std::unordered_map<RouterID, RouterLookupJob>;

    using PendingLookups = std::unordered_map<uint64_t, std::unique_ptr<IServiceLookup>>;

    using Sessions = std::unordered_multimap<Address, std::shared_ptr<OutboundContext>>;

    using SNodeSessions = std::unordered_map<RouterID, std::shared_ptr<exit::BaseSession>>;

    using ConvoMap = std::unordered_map<ConvoTag, Session>;

    /// set of outbound addresses to maintain to
    using OutboundSessions_t = std::unordered_set<Address>;

    using PathEnsureHook = std::function<void(Address, OutboundContext*)>;

    using LNSNameCache = std::unordered_map<std::string, std::pair<Address, llarp_time_t>>;

    struct OverheadStats
    {
      size_t overhead{};
      size_t total{};
      mutable llarp_time_t last_report{};

      void
      Clear()
      {
        overhead = 0;
        total = 0;
      }

      constexpr double
      percent() const
      {
        if (total)
          return (static_cast<double>(overhead) / static_cast<double>(total)) * 100;
        return 0;
      }

      template <typename T>
      void
      RecordOverhead(const T& t)
      {
        overhead += overhead_for(t);
        total += total_size_for(t);
      }

      bool
      ShouldReport() const;

      void Report(std::string_view) const;
    };
  }  // namespace service
}  // namespace llarp
