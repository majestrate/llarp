#include <llarp/util/alloc.h>
#include "sendcontext.hpp"

#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include "endpoint.hpp"
#include <utility>
#include <unordered_set>
#include <llarp/crypto/crypto.hpp>

namespace llarp
{
  namespace service
  {
    static constexpr size_t SendContextQueueSize = 512;

    SendContext::SendContext(
        ServiceInfo ident, const Introduction& intro, path::PathSet* send, Endpoint* ep)
        : remoteIdent(std::move(ident))
        , remoteIntro(intro)
        , m_PathSet(send)
        , m_DataHandler(ep)
        , m_Endpoint(ep)
        , createdAt(ep->Now())
        , m_SendQueue(SendContextQueueSize)
    {}

    bool
    SendContext::Send(std::shared_ptr<ProtocolFrame> msg, path::Path_ptr path)
    {
      if (path->IsReady()
          and m_SendQueue.tryPushBack(std::make_pair(
                  std::make_shared<routing::PathTransferMessage>(*msg, remoteIntro.pathID), path))
              == thread::QueueReturn::Success)
      {
        m_Endpoint->Router()->TriggerPump();
        return true;
      }
      return false;
    }

    void
    SendContext::FlushUpstream()
    {
      auto r = m_Endpoint->Router();
      auto rttRMS = 0ms;
      size_t num{};
      while (auto maybe = m_SendQueue.tryPopFront())
      {
        auto& [msg, path] = *maybe;
        msg->S = path->NextSeqNo();
        if (path->SendRoutingMessage(*msg, r))
        {
          m_Endpoint->m_Overhead.RecordOverhead(*msg);
          lastGoodSend = r->Now();
          m_Endpoint->ConvoTagTX(msg->T.T);
          const auto rtt = (path->intro.latency + remoteIntro.latency) * 2;
          rttRMS += rtt * rtt.count();
          ++num;
        }
      }
      if (num > 0)
        estimatedRTT =
            std::chrono::milliseconds{static_cast<int64_t>(std::sqrt(rttRMS.count() / num))};
    }

    /// send on an established convo tag
    void
    SendContext::EncryptAndSendTo(std::vector<std::vector<byte_t>> datas, ProtocolType t)
    {
      SharedSecret shared{};
      auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
      if (not path)
      {
        LogWarn(m_PathSet->Name(), " cannot encrypt and send: no path for intro ", remoteIntro);
        markedBad = true;
        return;
      }
      if (!m_DataHandler->GetCachedSessionKeyFor(currentConvoTag, shared))
      {
        LogWarn(
            m_PathSet->Name(),
            " could not send, has no cached session key on session T=",
            currentConvoTag);
        markedBad = true;
        return;
      }

      m_DataHandler->PutIntroFor(currentConvoTag, remoteIntro);
      m_DataHandler->PutReplyIntroFor(currentConvoTag, path->intro);

      Introduction introReply{};
      std::deque<ProtocolMessage> msgs;
      size_t idx{};
      for (auto& data : datas)
      {
        auto& msg = msgs.emplace_back();
        llarp_buffer_t buf{data};
        msg.PutBuffer(buf);
        msg.sender = m_Endpoint->GetIdentity().pub;
        msg.proto = t;
        if (auto maybe = m_Endpoint->GetSeqNoForConvo(currentConvoTag); maybe != std::nullopt)
          msg.seqno = *maybe;
        if (idx++)
          continue;
        msg.introReply = path->intro;
        introReply = path->intro;
      }

      m_Endpoint->Router()->QueueWork([introReply = std::move(introReply),
                                       msgs = std::move(msgs),
                                       shared,
                                       path,
                                       ident = m_Endpoint->GetIdentity(),
                                       this]() mutable {
        while (not msgs.empty())
        {
          auto f = std::make_shared<ProtocolFrame>();
          f->R = 0;
          Randomize(f->N);
          f->T = currentConvoTag;
          f->S = ++sequenceNo;
          f->F = introReply.pathID;
          if (not f->EncryptAndSign(msgs, shared, ident))
          {
            LogError(m_PathSet->Name(), " failed to sign message");
            return;
          }
          Send(f, path);
        }
      });
    }

    void
    SendContext::AsyncSendAuth(std::function<void(AuthResult)> resultHandler)
    {
      if (const auto maybe = m_Endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr()))
      {
        // send auth message
        const llarp_buffer_t authdata{maybe->token};
        AsyncGenIntro(authdata, ProtocolType::Auth);
        authResultListener = resultHandler;
      }
      else
        resultHandler({AuthResultCode::eAuthAccepted, "no auth needed"});
    }

    void
    SendContext::AsyncEncryptAndSendTo(std::vector<std::vector<byte_t>> msgs, ProtocolType protocol)
    {
      if (IntroSent())
      {
        EncryptAndSendTo(msgs, protocol);
        return;
      }
      // have we generated the initial intro but not sent it yet? bail here so we don't cause
      // bullshittery
      if (IntroGenerated() and not IntroSent())
      {
        LogWarn(
            m_PathSet->Name(),
            " we have generated an intial handshake but have not sent it yet so we drop a packet "
            "to prevent bullshittery");
        return;
      }
      const auto maybe = m_Endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr());
      if (maybe.has_value())
      {
        // send auth message
        const llarp_buffer_t authdata(maybe->token);
        AsyncGenIntro(authdata, ProtocolType::Auth);
      }
      else
      {
        llarp_buffer_t buf{msgs[0]};
        AsyncGenIntro(buf, protocol);
      }
    }
  }  // namespace service

}  // namespace llarp
