#include <llarp/util/alloc.h>
#include "dtls.hpp"
#include "linklayer.hpp"
#include "messages.hpp"
#include <llarp/util/logging.hpp>
#include <stdexcept>

namespace llarp::dtls
{
  static auto logcat = log::Cat("dtls");

  LinkLayer::LinkLayer(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work,
      bool permitInbound)
      : ILinkLayer{
            keyManager, loop, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, work}
      , m_Inbound{permitInbound}
  {}

  std::shared_ptr<ILinkSession>
  LinkLayer::NewOutboundSession(const RouterContact&, const AddressInfo&)
  {
    if (m_Inbound)
      throw std::logic_error{"inbound link cannot make outbound sessions"};
    log::warning(logcat, "dtls outbound session creation is not implemented");
    return nullptr;
  }

  std::string_view
  LinkLayer::Name() const
  {
    return "dtls";
  }

  uint16_t
  LinkLayer::Rank() const
  {
    return 2;
  }

  void
  LinkLayer::RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt)
  {
    DialbackFrame frame;
    llarp_buffer_t buf{pkt};
    if (not frame.BDecode(&buf) or buf.cur != buf.base + buf.sz)
    {
      log::warning(logcat, "dropping malformed dtls dialback frame from {}", from);
      return;
    }

    switch (frame.action)
    {
      case DialbackAction::Challenge:
        log::info(logcat, "received dtls dialback challenge from {}", from);
        break;
      case DialbackAction::Reply:
        log::info(logcat, "received dtls dialback reply from {}", from);
        break;
      case DialbackAction::Failure:
        log::warning(logcat, "received dtls dialback failure from {}: {}", from, frame.error);
        break;
    }

    log::warning(logcat, "dtls session handling is not implemented yet");
  }

  LinkLayer_ptr
  NewInboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work)
  {
    return std::make_shared<LinkLayer>(
        keyManager,
        loop,
        getrc,
        h,
        sign,
        before,
        est,
        reneg,
        timeout,
        closed,
        pumpDone,
        work,
        true);
  }

  LinkLayer_ptr
  NewOutboundLink(
      std::shared_ptr<KeyManager> keyManager,
      std::shared_ptr<EventLoop> loop,
      GetRCFunc getrc,
      LinkMessageHandler h,
      SignBufferFunc sign,
      BeforeConnectFunc_t before,
      SessionEstablishedHandler est,
      SessionRenegotiateHandler reneg,
      TimeoutHandler timeout,
      SessionClosedHandler closed,
      PumpDoneHandler pumpDone,
      WorkerFunc_t work)
  {
    return std::make_shared<LinkLayer>(
        keyManager,
        loop,
        getrc,
        h,
        sign,
        before,
        est,
        reneg,
        timeout,
        closed,
        pumpDone,
        work,
        false);
  }
}  // namespace llarp::dtls
