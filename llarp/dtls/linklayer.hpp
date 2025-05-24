#pragma once

#include <llarp/link/server.hpp>

namespace llarp::dtls
{

  struct LinkLayer : public ILinkLayer
  {
    LinkLayer(
        std::shared_ptr<KeyManager> keyManager,
        std::shared_ptr<EventLoop> ev,
        GetRCFunc getrc,
        LinkMessageHandler h,
        SignBufferFunc sign,
        BeforeConnectFunc_t before,
        SessionEstablishedHandler est,
        SessionRenegotiateHandler reneg,
        TimeoutHandler timeout,
        SessionClosedHandler closed,
        PumpDoneHandler pumpDone,
        WorkerFunc_t dowork,
        bool permitInbound);

    std::shared_ptr<ILinkSession>
    NewOutboundSession(const RouterContact& rc, const AddressInfo& ai) override;

    std::string_view
    Name() const override;

    uint16_t
    Rank() const override;

    void
    RecvFrom(const SockAddr& from, ILinkSession::Packet_t pkt) override;
  };

  using LinkLayer_ptr = std::shared_ptr<LinkLayer>;
}  // namespace llarp::dtls
