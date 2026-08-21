#include <llarp/util/alloc.h>
#include "dtls.hpp"
#include <llarp/iwp/iwp.hpp>

namespace llarp::dtls
{
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
    return iwp::NewInboundLink(
        keyManager, loop, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, work);
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
    return iwp::NewOutboundLink(
        keyManager, loop, getrc, h, sign, before, est, reneg, timeout, closed, pumpDone, work);
  }
}  // namespace llarp::dtls
