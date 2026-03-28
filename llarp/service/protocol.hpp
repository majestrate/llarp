#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/routing/message.hpp>
#include "protocol_type.hpp"
#include "identity.hpp"
#include "info.hpp"
#include "intro.hpp"
#include "handler.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/path/pathset.hpp>

#include <vector>

namespace llarp
{
  namespace path
  {
    /// forward declare
    struct Path;
  }  // namespace path

  namespace service
  {
    struct Endpoint;

    /// inner message
    struct ProtocolMessage
    {
      ProtocolMessage(const ConvoTag& tag);
      ProtocolMessage();
      ~ProtocolMessage();
      ProtocolType proto = ProtocolType::TrafficV4;
      llarp_time_t queued = 0s;
      std::vector<byte_t> payload;
      Introduction introReply{};
      ServiceInfo sender{};
      Endpoint* handler = nullptr;
      ConvoTag tag{};
      uint64_t seqno = 0;
      uint64_t version = llarp::constants::proto_version;

      /// encode metainfo for lmq endpoint auth
      std::vector<char>
      EncodeAuthInfo() const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val);

      bool
      BEncode(llarp_buffer_t* buf) const;

      bool
      BDecode(llarp_buffer_t* buf);

      void
      PutBuffer(const llarp_buffer_t& payload);

      static void
      ProcessAsync(path::Path_ptr p, PathID_t from, std::shared_ptr<ProtocolMessage> self);

      bool
      operator>(const ProtocolMessage& other) const
      {
        return seqno > other.seqno;
      }
    };

    /// outer message
    struct ProtocolFrame final : routing::IMessage
    {
      using Encrypted_t = Encrypted<constants::service_proto_message_max_size>;
      PQCipherBlock C{};
      Encrypted_t D{};
      uint64_t R;
      KeyExchangeNonce N{};
      Signature Z{};
      PathID_t F{};
      ConvoTag T{};

      size_t
      overhead() const noexcept override;

      ProtocolFrame(const ProtocolFrame& other)
          : IMessage{}
          , C(other.C)
          , D(other.D)
          , R(other.R)
          , N(other.N)
          , Z(other.Z)
          , F(other.F)
          , T(other.T)
      {
        S = other.S;
        version = other.version;
      }

      ProtocolFrame() : IMessage{}
      {
        Clear();
      }

      ~ProtocolFrame() override;

      bool
      operator==(const ProtocolFrame& other) const;

      bool
      operator!=(const ProtocolFrame& other) const
      {
        return !(*this == other);
      }

      ProtocolFrame&
      operator=(const ProtocolFrame& other);

      bool
      EncryptAndSign(
          const ProtocolMessage& msg, const SharedSecret& sharedkey, const Identity& localIdent);

      bool
      EncryptAndSign(
          std::deque<ProtocolMessage>& msgs,
          const SharedSecret& sharedkey,
          const Identity& localIdent);

      bool
      Sign(const Identity& localIdent);

      bool
      AsyncDecryptAndVerify(
          EventLoop_ptr loop,
          path::Path_ptr fromPath,
          const Identity& localIdent,
          Endpoint* handler,
          std::function<void(std::shared_ptr<ProtocolMessage>)> hook = nullptr) const;

      bool
      DecryptPayloadInto(const SharedSecret& sharedkey, std::vector<ProtocolMessage>& into) const;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      BDecode(llarp_buffer_t* buf)
      {
        return bencode_decode_dict(*this, buf);
      }

      void
      Clear() override
      {
        Zero(C);
        D.Clear();
        F.Zero();
        T.Zero();
        N.Zero();
        Z.Zero();
        R = 0;
        version = constants::proto_version;
      }

      bool
      Verify(const ServiceInfo& from) const;

      bool
      HandleMessage(routing::IMessageHandler* h, AbstractRouter* r) const override;
    };

  }  // namespace service

  template <>
  constexpr inline bool is_aligned_buffer<service::ProtocolFrame::Encrypted_t> = true;
}  // namespace llarp
