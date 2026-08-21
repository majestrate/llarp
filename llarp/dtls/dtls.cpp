#include <llarp/util/alloc.h>
#include "dtls.hpp"
#include "linklayer.hpp"
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
  LinkLayer::RecvFrom(const SockAddr&, ILinkSession::Packet_t)
  {
    log::warning(logcat, "dtls recv path is not implemented");
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
