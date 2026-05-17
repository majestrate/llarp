#pragma once

#include <llarp/crypto/types.hpp>
#include "policy.hpp"
#include <llarp/routing/message.hpp>

#include <vector>

namespace llarp::routing
{
  struct ObtainExitMessage final : public IMessage
  {
    std::vector<exit::Policy> B;
    uint64_t E{0};
    PubKey I;
    uint64_t T{0};
    std::vector<exit::Policy> W;
    uint64_t X{0};
    Signature Z;

    ObtainExitMessage() : IMessage()
    {}

    ~ObtainExitMessage() override = default;

    void
    Clear() override
    {
      B.clear();
      E = 0;
      I.Zero();
      T = 0;
      W.clear();
      X = 0;
      Z.Zero();
    }

    /// populates I and signs
    bool
    Sign(const SecretKey& sk);

    bool
    Verify() const;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
  };

  struct GrantExitMessage final : IMessage
  {
    using Nonce_t = AlignedBuffer<16>;

    uint64_t T;
    Nonce_t Y;
    Signature Z;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    Sign(const SecretKey& sk);

    bool
    Verify(const PubKey& pk) const;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

    void
    Clear() override
    {
      T = 0;
      Zero(Y);
      Z.Zero();
    }
  };

  struct RejectExitMessage final : IMessage
  {
    using Nonce_t = AlignedBuffer<16>;
    uint64_t B;
    std::vector<exit::Policy> R;
    uint64_t T;
    Nonce_t Y;
    Signature Z;

    void
    Clear() override
    {
      B = 0;
      R.clear();
      T = 0;
      Zero(Y);
      Z.Zero();
    }

    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify(const llarp::PubKey& pk) const;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
  };

  struct UpdateExitVerifyMessage final : IMessage
  {
    using Nonce_t = AlignedBuffer<16>;
    uint64_t T;
    Nonce_t Y;
    Signature Z;

    ~UpdateExitVerifyMessage() override = default;

    void
    Clear() override
    {
      T = 0;
      Zero(Y);
      Z.Zero();
    }

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;
  };

  struct UpdateExitMessage final : IMessage
  {
    using Nonce_t = AlignedBuffer<16>;
    PathID_t P;
    uint64_t T;
    Nonce_t Y;
    Signature Z;

    bool
    Sign(const SecretKey& sk);

    bool
    Verify(const PubKey& pk) const;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

    void
    Clear() override
    {
      P.Zero();
      T = 0;
      Zero(Y);
      Z.Zero();
    }
  };

  struct CloseExitMessage final : IMessage
  {
    using Nonce_t = AlignedBuffer<16>;

    Nonce_t Y;
    Signature Z;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

    bool
    Sign(const SecretKey& sk);

    bool
    Verify(const PubKey& pk) const;

    void
    Clear() override
    {
      Zero(Y);
      Z.Zero();
    }
  };
}  // namespace llarp::routing
