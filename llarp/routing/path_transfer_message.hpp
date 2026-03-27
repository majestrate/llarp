#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include "message.hpp"
#include <llarp/service/protocol.hpp>

namespace llarp::routing
{

  struct PathTransferMessage final : public IMessage
  {
    PathID_t P;
    service::ProtocolFrame T;
    TunnelNonce Y;

    PathTransferMessage() = default;
    PathTransferMessage(const service::ProtocolFrame& f, const PathID_t& p) : P(p), T(f)
    {
      Randomize(Y);
    }
    ~PathTransferMessage() override = default;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(IMessageHandler*, AbstractRouter* r) const override;

    void
    Clear() override
    {
      P.Zero();
      T.Clear();
      Zero(Y);
      version = 0;
    }

    size_t
    overhead() const noexcept override;
  };

}  // namespace llarp::routing
