#include <llarp/util/alloc.h>
#include "path.hpp"

#include <llarp/dht/context.hpp>
#include <llarp/exit/context.hpp>
#include <llarp/exit/exit_messages.hpp>
#include <llarp/link/i_link_manager.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/messages/relay_commit.hpp>
#include <llarp/messages/relay_status.hpp>
#include "path_context.hpp"
#include "transit_hop.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_latency_message.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/util/buffer.hpp>

#include <oxenc/endian.h>

namespace llarp
{
  namespace path
  {
    std::string
    TransitHopInfo::ToString() const
    {
      return fmt::format(
          "[TransitHopInfo tx={} rx={} upstream={} downstream={}]",
          txID,
          rxID,
          upstream,
          downstream);
    }

    TransitHop::TransitHop() : IHopHandler{}
    {}

    bool
    TransitHop::Expired(llarp_time_t now) const
    {
      return destroy || (now >= ExpireTime());
    }

    llarp_time_t
    TransitHop::ExpireTime() const
    {
      return started + lifetime;
    }

    bool
    TransitHop::HandleLRSM(
        uint64_t status, std::array<EncryptedFrame, 8>& frames, AbstractRouter* r)
    {
      auto msg = std::make_shared<LR_StatusMessage>(frames);
      msg->status = status;
      msg->pathid = info.rxID;

      // TODO: add to IHopHandler some notion of "path status"

      const uint64_t ourStatus = LR_StatusRecord::SUCCESS;

      msg->AddFrame(pathKey, ourStatus);
      LR_StatusMessage::QueueSendMessage(r, info.downstream, msg, shared_from_this());
      return true;
    }

    TransitHopInfo::TransitHopInfo(const RouterID& down, const LR_CommitRecord& record)
        : txID(record.txid), rxID(record.rxid), upstream(record.nextHop), downstream(down)
    {}

    bool
    TransitHop::SendRoutingMessage(const routing::IMessage& msg, AbstractRouter* r)
    {
      if (!IsEndpoint(r->pubkey()))
        return false;

      std::array<byte_t, MAX_LINK_MSG_SIZE - 128> tmp{};
      llarp_buffer_t buf(tmp);
      if (!msg.BEncode(&buf))
      {
        llarp::LogError("failed to encode routing message");
        return false;
      }
      TunnelNonce N;
      N.Randomize();
      buf.sz = buf.cur - buf.base;
      // pad to nearest MESSAGE_PAD_SIZE bytes
      auto dlt = buf.sz % pad_size;
      if (dlt)
      {
        dlt = pad_size - dlt;
        // randomize padding
        CryptoManager::instance()->randbytes(buf.cur, dlt);
        buf.sz += dlt;
      }
      buf.cur = buf.base;
      r->pathContext().FlushDownstreamLater(weak_from_this());
      return HandleDownstream(buf, N, r);
    }

    void
    TransitWorker::RunDownstream()
    {
      util::SetThreadName("llarp-down");
      while (m_DownstreamSubmit.enabled())
      {
        auto maybe = m_DownstreamSubmit.popFrontWithTimeout(1s);
        if (not maybe)
          continue;
        auto& ev = maybe->second;
        auto transit_hop_ptr = maybe->first.lock();
        RelayDownstreamMessage msg{};
        const llarp_buffer_t buf(ev.first);
        msg.pathid = transit_hop_ptr->info.rxID;
        msg.Y = ev.second ^ transit_hop_ptr->nonceXOR;
        CryptoManager::instance()->xchacha20(buf, transit_hop_ptr->pathKey, ev.second);
        msg.X = buf;
        if (m_DownstreamGather.tryPushBack(std::make_pair(maybe->first, msg))
            == thread::QueueReturn::Success)
          m_Wakeup->Trigger();
      }
    }

    void
    TransitWorker::RunUpstream()
    {
      util::SetThreadName("llarp-up");
      while (m_UpstreamSubmit.enabled())
      {
        auto maybe = m_UpstreamSubmit.popFrontWithTimeout(1s);
        if (not maybe)
          continue;
        auto transit_hop_ptr = maybe->first.lock();
        auto& ev = maybe->second;
        const llarp_buffer_t buf(ev.first);
        RelayUpstreamMessage msg{};
        CryptoManager::instance()->xchacha20(buf, transit_hop_ptr->pathKey, ev.second);
        msg.pathid = transit_hop_ptr->info.txID;
        msg.Y = ev.second ^ transit_hop_ptr->nonceXOR;
        msg.X = buf;
        if (m_UpstreamGather.tryPushBack(std::make_pair(maybe->first, msg))
            == thread::QueueReturn::Success)
          m_Wakeup->Trigger();
      }
    }

    void
    TransitWorker::Gather()
    {
      auto* r = m_PathContext.Router();

      std::unordered_map<TransitHop_ptr, std::vector<RelayUpstreamMessage>> upstream;
      while (auto maybe = m_UpstreamGather.tryPopFront())
      {
        if (auto transit_hop = maybe->first.lock())
        {
          upstream[transit_hop].emplace_back(std::move(maybe->second));
        }
      }

      for (auto& [ptr, msgs] : upstream)
        ptr->HandleAllUpstream(msgs, r);

      std::unordered_map<TransitHop_ptr, std::vector<RelayDownstreamMessage>> downstream;
      while (auto maybe = m_DownstreamGather.tryPopFront())
      {
        if (auto transit_hop = maybe->first.lock())
        {
          downstream[transit_hop].emplace_back(std::move(maybe->second));
        }
      }
      for (auto& [ptr, msgs] : downstream)
        ptr->HandleAllDownstream(msgs, r);
      r->TriggerPump();
    }

    void
    TransitWorker::SubmitDownstream(std::weak_ptr<TransitHop> hop, IHopHandler::TrafficEvent_t ev)
    {
      m_DownstreamSubmit.tryPushBack(std::make_pair(hop, ev));
    }

    void
    TransitWorker::SubmitUpstream(std::weak_ptr<TransitHop> hop, IHopHandler::TrafficEvent_t ev)
    {
      m_UpstreamSubmit.tryPushBack(std::make_pair(hop, ev));
    }

    void
    TransitWorker::Start(size_t upstream_threads, size_t downstream_threads)
    {
      if (upstream_threads == 0)
        upstream_threads = 1;
      if (downstream_threads == 0)
        downstream_threads = 1;

      while (upstream_threads > 0)
      {
        m_Threads.emplace_back([self = this]() { self->RunUpstream(); });
        upstream_threads--;
      }
      while (downstream_threads > 0)
      {
        m_Threads.emplace_back([self = this]() { self->RunDownstream(); });
        downstream_threads--;
      }
    }

    TransitWorker::TransitWorker(PathContext& ctx, const EventLoop_ptr& loop)
        : m_UpstreamSubmit{queue_length}
        , m_UpstreamGather{queue_length}
        , m_DownstreamSubmit{queue_length}
        , m_DownstreamGather{queue_length}
        , m_PathContext{ctx}
    {
      m_Wakeup = loop->make_waker([this]() { Gather(); });
    }

    TransitWorker::~TransitWorker()
    {
      m_UpstreamSubmit.disable();
      m_DownstreamSubmit.disable();
      for (auto& th : m_Threads)
        th.join();
    }

    void
    TransitHop::HandleAllUpstream(std::vector<RelayUpstreamMessage> msgs, AbstractRouter* r)
    {
      if (IsEndpoint(r->pubkey()))
      {
        for (const auto& msg : msgs)
        {
          const llarp_buffer_t buf(msg.X);
          if (!r->ParseRoutingMessageBuffer(buf, this, info.rxID))
          {
            LogWarn("invalid upstream data on endpoint ", info);
          }
          m_LastActivity = r->Now();
        }
        r->pathContext().FlushDownstreamLater(weak_from_this());
        return;
      }
      for (const auto& msg : msgs)
      {
        llarp::LogDebug(
            "relay ",
            msg.X.size(),
            " bytes upstream from ",
            info.downstream,
            " to ",
            info.upstream);
        r->SendToOrQueue(info.upstream, msg);
      }
    }

    void
    TransitHop::HandleAllDownstream(std::vector<RelayDownstreamMessage> msgs, AbstractRouter* r)
    {
      for (const auto& msg : msgs)
      {
        llarp::LogDebug(
            "relay ",
            msg.X.size(),
            " bytes downstream from ",
            info.upstream,
            " to ",
            info.downstream);
        r->SendToOrQueue(info.downstream, msg);
      }
    }

    void
    TransitHop::FlushUpstream(AbstractRouter* r)
    {
      if (m_UpstreamQueue.empty())
        return;
      auto self = weak_from_this();
      for (auto& ev : m_UpstreamQueue)
        r->transitWorker().SubmitUpstream(self, std::move(ev));

      m_UpstreamQueue.clear();
    }

    void
    TransitHop::FlushDownstream(AbstractRouter* r)
    {
      if (m_DownstreamQueue.empty())
        return;
      auto self = weak_from_this();
      for (auto& ev : m_DownstreamQueue)
        r->transitWorker().SubmitDownstream(self, std::move(ev));

      m_DownstreamQueue.clear();
    }

    /// this is where a DHT message is handled at the end of a path, that is,
    /// where a SNode receives a DHT message from a client along a path.
    bool
    TransitHop::HandleDHTMessage(const llarp::dht::IMessage& msg, AbstractRouter* r)
    {
      return r->dht()->impl->RelayRequestForPath(info.rxID, msg);
    }

    bool
    TransitHop::HandlePathLatencyMessage(
        const llarp::routing::PathLatencyMessage& msg, AbstractRouter* r)
    {
      llarp::routing::PathLatencyMessage reply;
      reply.L = msg.T;
      reply.S = msg.S;
      return SendRoutingMessage(reply, r);
    }

    bool
    TransitHop::HandlePathConfirmMessage(
        [[maybe_unused]] const llarp::routing::PathConfirmMessage& msg,
        [[maybe_unused]] AbstractRouter* r)
    {
      llarp::LogWarn("unwarranted path confirm message on ", info);
      return false;
    }

    bool
    TransitHop::HandleDataDiscardMessage(
        [[maybe_unused]] const llarp::routing::DataDiscardMessage& msg,
        [[maybe_unused]] AbstractRouter* r)
    {
      llarp::LogWarn("unwarranted path data discard message on ", info);
      return false;
    }

    bool
    TransitHop::HandleObtainExitMessage(
        const llarp::routing::ObtainExitMessage& msg, AbstractRouter* r)
    {
      if (msg.Verify() && r->exitContext().ObtainNewExit(msg.I, info.rxID, msg.E != 0))
      {
        llarp::routing::GrantExitMessage grant;
        grant.S = NextSeqNo();
        grant.T = msg.T;
        if (!grant.Sign(r->identity()))
        {
          llarp::LogError("Failed to sign grant exit message");
          return false;
        }
        return SendRoutingMessage(grant, r);
      }
      // TODO: exponential backoff
      // TODO: rejected policies
      llarp::routing::RejectExitMessage reject;
      reject.S = NextSeqNo();
      reject.T = msg.T;
      if (!reject.Sign(r->identity()))
      {
        llarp::LogError("Failed to sign reject exit message");
        return false;
      }
      return SendRoutingMessage(reject, r);
    }

    bool
    TransitHop::HandleCloseExitMessage(
        const llarp::routing::CloseExitMessage& msg, AbstractRouter* r)
    {
      const llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
      auto ep = r->exitContext().FindEndpointForPath(info.rxID);
      if (ep && msg.Verify(ep->PubKey()))
      {
        llarp::routing::CloseExitMessage reply;
        reply.Y = msg.Y;
        reply.S = NextSeqNo();
        if (reply.Sign(r->identity()))
        {
          if (SendRoutingMessage(reply, r))
          {
            ep->Close();
            return true;
          }
        }
      }
      return SendRoutingMessage(discard, r);
    }

    bool
    TransitHop::HandleUpdateExitVerifyMessage(
        const llarp::routing::UpdateExitVerifyMessage& msg, AbstractRouter* r)
    {
      (void)msg;
      (void)r;
      llarp::LogError("unwarranted exit verify on ", info);
      return false;
    }

    bool
    TransitHop::HandleUpdateExitMessage(
        const llarp::routing::UpdateExitMessage& msg, AbstractRouter* r)
    {
      auto ep = r->exitContext().FindEndpointForPath(msg.P);
      if (ep)
      {
        if (!msg.Verify(ep->PubKey()))
          return false;

        if (ep->UpdateLocalPath(info.rxID))
        {
          llarp::routing::UpdateExitVerifyMessage reply;
          reply.T = msg.T;
          reply.S = NextSeqNo();
          return SendRoutingMessage(reply, r);
        }
      }
      // on fail tell message was discarded
      llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
      return SendRoutingMessage(discard, r);
    }

    bool
    TransitHop::HandleRejectExitMessage(
        const llarp::routing::RejectExitMessage& msg, AbstractRouter* r)
    {
      (void)msg;
      (void)r;
      llarp::LogError(info, " got unwarranted RXM");
      return false;
    }

    bool
    TransitHop::HandleGrantExitMessage(
        const llarp::routing::GrantExitMessage& msg, AbstractRouter* r)
    {
      (void)msg;
      (void)r;
      llarp::LogError(info, " got unwarranted GXM");
      return false;
    }

    bool
    TransitHop::HandleTransferTrafficMessage(
        const llarp::routing::TransferTrafficMessage& msg, AbstractRouter* r)
    {
      auto endpoint = r->exitContext().FindEndpointForPath(info.rxID);
      if (endpoint)
      {
        bool sent = true;
        for (const auto& pkt : msg.X)
        {
          // check short packet buffer
          if (pkt.size() <= 8)
            continue;
          auto counter = oxenc::load_big_to_host<uint64_t>(pkt.data());
          llarp_buffer_t buf{pkt.data() + 8, pkt.size() - 8};
          sent =
              endpoint->QueueOutboundTraffic(info.rxID, buf.copy(), counter, msg.protocol) and sent;
        }
        return sent;
      }

      llarp::LogError("No exit endpoint on ", info);
      // discarded
      llarp::routing::DataDiscardMessage discard(info.rxID, msg.S);
      return SendRoutingMessage(discard, r);
    }

    bool
    TransitHop::HandlePathTransferMessage(
        const llarp::routing::PathTransferMessage& msg, AbstractRouter* r)
    {
      auto path = r->pathContext().GetPathForTransfer(msg.P);
      llarp::routing::DataDiscardMessage discarded{msg.P, msg.S};
      if (path == nullptr || msg.T.F != info.txID)
      {
        return SendRoutingMessage(discarded, r);
      }
      // send routing message
      if (path->SendRoutingMessage(msg.T, r))
      {
        r->pathContext().FlushDownstreamLater(path);
        return true;
      }
      return SendRoutingMessage(discarded, r);
    }

    std::string
    TransitHop::ToString() const
    {
      return fmt::format(
          "[TransitHop {} started={} lifetime={}", info, started.count(), lifetime.count());
    }

    void
    TransitHop::Stop()
    {}

    void
    TransitHop::SetSelfDestruct()
    {
      destroy = true;
    }

    void
    TransitHop::QueueDestroySelf(AbstractRouter* r)
    {
      r->loop()->call([self = shared_from_this()] { self->SetSelfDestruct(); });
    }
  }  // namespace path
}  // namespace llarp
