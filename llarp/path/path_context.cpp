#include <llarp/util/alloc.h>
#include "path_context.hpp"

#include <llarp/messages/relay_commit.hpp>
#include "llarp/net/sock_addr.hpp"
#include "path.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_outbound_message_handler.hpp>

namespace std
{
  auto
  operator<(
      const std::shared_ptr<llarp::path::TransitHop>& lhs,
      const std::shared_ptr<llarp::path::TransitHop>& rhs) -> bool
  {
    return lhs->info.txID.as_array() < rhs->info.txID.as_array();
  }

  auto
  operator<(const llarp::PathID_t& lhs, const llarp::PathID_t& rhs) -> bool
  {
    return lhs.as_array() < rhs.as_array();
  }

  auto
  operator<(const std::shared_ptr<llarp::path::TransitHop>& hop, const llarp::PathID_t& txid)
      -> bool
  {
    return hop->info.txID < txid;
  }

  auto
  operator<(const llarp::PathID_t& txid, const std::shared_ptr<llarp::path::TransitHop>& hop)
      -> bool
  {
    return txid < hop->info.txID;
  }
}  // namespace std
namespace llarp
{
  namespace path
  {
    static constexpr auto DefaultPathBuildLimit = 500ms;

    PathContext::PathContext(AbstractRouter* router)
        : m_Router(router), m_AllowTransit(false), m_PathLimits(DefaultPathBuildLimit)
    {
      m_FlushLater = loop()->make_waker([this]() { FlushDeferred(); });
    }

    void
    PathContext::FlushDeferred()
    {
      for (const auto& h : m_FlushUpstreamQueue)
      {
        if (auto ptr = h.lock())
        {
          ptr->FlushUpstream(m_Router);
        }
      }
      m_FlushUpstreamQueue.clear();

      for (const auto& h : m_FlushDownstreamQueue)
      {
        if (auto ptr = h.lock())
        {
          ptr->FlushDownstream(m_Router);
        }
      }
      m_FlushUpstreamQueue.clear();
    }

    void
    PathContext::AllowTransit()
    {
      m_AllowTransit = true;
    }

    bool
    PathContext::AllowingTransit() const
    {
      return m_AllowTransit;
    }

    bool
    PathContext::CheckPathLimitHitByAddr(const SockAddr& addr)
    {
#ifdef TESTNET
      return false;
#else
      // try inserting remote address by ip into decaying hash set
      // if it cannot insert it has hit a limit
      return not m_PathLimits.Insert(addr.getIP());
#endif
    }

    const EventLoop_ptr&
    PathContext::loop()
    {
      return m_Router->loop();
    }

    const SecretKey&
    PathContext::EncryptionSecretKey()
    {
      return m_Router->encryption();
    }

    bool
    PathContext::HopIsUs(const RouterID& k) const
    {
      return std::equal(m_Router->pubkey(), m_Router->pubkey() + PUBKEYSIZE, k.begin());
    }

    PathContext::EndpointPathPtrSet
    PathContext::FindOwnedPathsWithEndpoint(const RouterID& r)
    {
      EndpointPathPtrSet found;
      m_OurPaths.ForEach([&](const Path_ptr& p) {
        if (p->Endpoint() == r && p->IsReady())
          found.insert(p);
      });
      return found;
    }

    bool
    PathContext::ForwardLRCM(
        const RouterID& nextHop,
        const std::array<EncryptedFrame, 8>& frames,
        SendStatusHandler handler)
    {
      if (handler == nullptr)
      {
        LogError("Calling ForwardLRCM without passing result handler");
        return false;
      }

      const LR_CommitMessage msg{frames};

      LogDebug("forwarding LRCM to ", nextHop);

      return m_Router->SendToOrQueue(nextHop, msg, handler);
    }

    template <
        typename Lock_t,
        typename Map_t,
        typename Key_t,
        typename CheckValue_t,
        typename GetFunc_t,
        typename Return_ptr = HopHandler_ptr>
    Return_ptr
    MapGet(Map_t& map, const Key_t& k, CheckValue_t check, GetFunc_t get)
    {
      Lock_t lock(map.first);
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second; ++i)
      {
        if (check(i->second))
          return get(i->second);
      }
      return nullptr;
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename CheckValue_t>
    bool
    MapHas(Map_t& map, const Key_t& k, CheckValue_t check)
    {
      Lock_t lock(map.first);
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second; ++i)
      {
        if (check(i->second))
          return true;
      }
      return false;
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename Value_t>
    void
    MapPut(Map_t& map, const Key_t& k, const Value_t& v)
    {
      Lock_t lock(map.first);
      map.second.emplace(k, v);
    }

    template <typename Lock_t, typename Map_t, typename Visit_t>
    void
    MapIter(Map_t& map, Visit_t v)
    {
      Lock_t lock(map.first);
      for (const auto& item : map.second)
        v(item);
    }

    template <typename Lock_t, typename Map_t, typename Key_t, typename Check_t>
    void
    MapDel(Map_t& map, const Key_t& k, Check_t check)
    {
      Lock_t lock(map.first);
      auto range = map.second.equal_range(k);
      for (auto i = range.first; i != range.second;)
      {
        if (check(i->second))
          i = map.second.erase(i);
        else
          ++i;
      }
    }

    void
    PathContext::AddOwnPath(PathSet_ptr set, Path_ptr path)
    {
      set->AddPath(path);
      MapPut<util::Lock>(m_OurPaths, path->TXID(), path);
      MapPut<util::Lock>(m_OurPaths, path->RXID(), path);
    }

    bool
    CompareTransitHop::compare(const PathID_t& lhs, const PathID_t& rhs) const
    {
      return lhs < rhs;
    }

    bool
    PathContext::HasTransitHop(const TransitHopInfo& info)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      auto [begin, end] = m_TransitPaths.second.equal_range(info.txID);
      while (begin != end)
      {
        if ((*begin)->info == info)
          return true;
        ++begin;
      }
      return false;
    }

    std::optional<std::weak_ptr<TransitHop>>
    PathContext::TransitHopByInfo(const TransitHopInfo& info)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      auto [begin, end] = m_TransitPaths.second.equal_range(info.txID);
      while (begin != end)
      {
        if ((*begin)->info == info)
          return (*begin)->weak_from_this();
        ++begin;
      }
      return std::nullopt;
    }

    std::optional<std::weak_ptr<TransitHop>>
    PathContext::TransitHopByUpstream(const RouterID& upstream, const PathID_t& id)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      auto [begin, end] = m_TransitPaths.second.equal_range(id);
      while (begin != end)
      {
        if ((*begin)->info.upstream == upstream)
          return (*begin)->weak_from_this();
        ++begin;
      }
      return std::nullopt;
    }

    HopHandler_ptr
    PathContext::GetByUpstream(const RouterID& remote, const PathID_t& id)
    {
      auto own = MapGet<util::Lock>(
          m_OurPaths,
          id,
          [](const Path_ptr) -> bool {
            // TODO: is this right?
            return true;
          },
          [](Path_ptr p) -> HopHandler_ptr { return p; });
      if (own)
        return own;

      if (auto maybe = TransitHopByUpstream(remote, id))
        return maybe->lock();
      return nullptr;
    }

    bool
    PathContext::TransitHopPreviousIsRouter(const PathID_t& path, const RouterID& otherRouter)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      auto [begin, end] = m_TransitPaths.second.equal_range(path);
      while (begin != end)
      {
        if ((*begin)->info.downstream == otherRouter)
          return true;
        ++begin;
      }
      return false;
    }

    HopHandler_ptr
    PathContext::GetByDownstream(const RouterID& remote, const PathID_t& id)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      auto [begin, end] = m_TransitPaths.second.equal_range(id);
      while (begin != end)
      {
        if ((*begin)->info.downstream == remote)
          return *begin;
        ++begin;
      }
      return nullptr;
    }

    PathSet_ptr
    PathContext::GetLocalPathSet(const PathID_t& id)
    {
      auto& map = m_OurPaths;
      util::Lock lock(map.first);
      auto itr = map.second.find(id);
      if (itr != map.second.end())
      {
        if (auto parent = itr->second->m_PathSet.lock())
          return parent;
      }
      return nullptr;
    }

    const byte_t*
    PathContext::OurRouterID() const
    {
      return m_Router->pubkey();
    }

    AbstractRouter*
    PathContext::Router()
    {
      return m_Router;
    }

    TransitHop_ptr
    PathContext::GetPathForTransfer(const PathID_t& id)
    {
      const RouterID us(OurRouterID());
      auto& map = m_TransitPaths;
      {
        SyncTransitMap_t::Lock_t lock(map.first);
        auto range = map.second.equal_range(id);
        for (auto i = range.first; i != range.second; ++i)
        {
          if ((*i)->info.upstream == us)
            return *i;
        }
      }
      return nullptr;
    }

    void
    PathContext::PumpUpstream()
    {
      m_TransitPaths.ForEach([&](auto& ptr) { ptr->FlushUpstream(m_Router); });
      m_OurPaths.ForEach([&](auto& ptr) { ptr->FlushUpstream(m_Router); });
    }

    void
    PathContext::PumpDownstream()
    {
      m_TransitPaths.ForEach([&](auto& ptr) { ptr->FlushDownstream(m_Router); });
      m_OurPaths.ForEach([&](auto& ptr) { ptr->FlushDownstream(m_Router); });
    }

    uint64_t
    PathContext::CurrentTransitPaths()
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      const auto& map = m_TransitPaths.second;
      return map.size() / 2;
    }

    uint64_t
    PathContext::CurrentOwnedPaths(path::PathStatus st)
    {
      uint64_t num{};
      util::Lock lock{m_OurPaths.first};
      auto& map = m_OurPaths.second;
      for (auto itr = map.begin(); itr != map.end(); ++itr)
      {
        if (itr->second->Status() == st)
          num++;
      }
      return num / 2;
    }

    void
    PathContext::FlushUpstreamLater(std::weak_ptr<IHopHandler> hop)
    {
      m_FlushUpstreamQueue.emplace_back(std::move(hop));
      m_FlushLater->Trigger();
    }

    void
    PathContext::FlushDownstreamLater(std::weak_ptr<IHopHandler> hop)
    {
      m_FlushDownstreamQueue.emplace_back(std::move(hop));
      m_FlushLater->Trigger();
    }

    void
    PathContext::PutTransitHop(std::shared_ptr<TransitHop> hop)
    {
      SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
      m_TransitPaths.second.emplace(hop);
    }

    void
    PathContext::ExpirePaths(llarp_time_t now)
    {
      // decay limits
      m_PathLimits.Decay(now);

      {
        SyncTransitMap_t::Lock_t lock(m_TransitPaths.first);
        auto& map = m_TransitPaths.second;
        auto itr = map.begin();
        while (itr != map.end())
        {
          (*itr)->DecayFilters(now);
          if ((*itr)->Expired(now))
          {
            m_Router->outboundMessageHandler().RemovePath((*itr)->info.txID);
            m_Router->outboundMessageHandler().RemovePath((*itr)->info.rxID);
            itr = map.erase(itr);
          }
          else
          {
            ++itr;
          }
        }
      }
      {
        util::Lock lock(m_OurPaths.first);
        auto& map = m_OurPaths.second;
        auto itr = map.begin();
        while (itr != map.end())
        {
          itr->second->DecayFilters(now);
          if (itr->second->Expired(now))
          {
            itr = map.erase(itr);
          }
          else
          {
            ++itr;
          }
        }
      }
    }

    routing::MessageHandler_ptr
    PathContext::GetHandler(const PathID_t& id)
    {
      routing::MessageHandler_ptr h = nullptr;
      auto pathset = GetLocalPathSet(id);
      if (pathset)
      {
        h = pathset->GetPathByID(id);
      }
      if (h)
        return h;
      const RouterID us(OurRouterID());
      auto& map = m_TransitPaths;
      {
        SyncTransitMap_t::Lock_t lock(map.first);
        auto range = map.second.equal_range(id);
        for (auto i = range.first; i != range.second; ++i)
        {
          if ((*i)->info.upstream == us)
            return *i;
        }
      }
      return nullptr;
    }

    void
    PathContext::RemovePathSet(PathSet_ptr)
    {}
  }  // namespace path
}  // namespace llarp
