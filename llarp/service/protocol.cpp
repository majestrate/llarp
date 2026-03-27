#include <llarp/util/alloc.h>
#include "protocol.hpp"
#include <llarp/path/path.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/mem.hpp>
#include <llarp/util/meta/memfn.hpp>
#include "endpoint.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <utility>

namespace llarp
{
  namespace service
  {
    static auto logcat = log::Cat("hsproto");

    ProtocolMessage::ProtocolMessage()
    {
      tag.Zero();
    }

    ProtocolMessage::ProtocolMessage(const ConvoTag& t) : tag(t)
    {}

    ProtocolMessage::~ProtocolMessage() = default;

    void
    ProtocolMessage::PutBuffer(const llarp_buffer_t& buf)
    {
      payload.resize(buf.sz);
      memcpy(payload.data(), buf.base, buf.sz);
    }

    void
    ProtocolMessage::ProcessAsync(
        path::Path_ptr path, PathID_t from, std::shared_ptr<ProtocolMessage> self)
    {
      if (self == nullptr or self->handler == nullptr)
      {
        log::error(
            logcat,
            "invalid message path={} handler={}",
            self != nullptr,
            self->handler != nullptr);
        return;
      }
      if (!self->handler->HandleDataMessage(path, from, self))
        LogWarn("failed to handle data message from ", path->Name());
    }

    bool
    ProtocolMessage::BDecode(llarp_buffer_t* buf)
    {
      return bencode_decode_dict(*this, buf);
    }

    bool
    ProtocolMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("a", proto, read, k, buf))
        return false;
      if (k.startswith("d"))
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(buf, &strbuf))
          return false;
        PutBuffer(strbuf);
        return true;
      }
      if (!BEncodeMaybeReadDictEntry("i", introReply, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("n", seqno, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("s", sender, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("t", tag, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("v", version, read, k, buf))
        return false;
      return read;
    }

    bool
    ProtocolMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictInt("a", proto, buf))
        return false;
      if (not payload.empty())
      {
        if (!bencode_write_bytestring(buf, "d", 1))
          return false;
        if (!bencode_write_bytestring(buf, payload.data(), payload.size()))
          return false;
      }
      if (!BEncodeWriteDictEntry("i", introReply, buf))
        return false;
      if (!BEncodeWriteDictInt("n", seqno, buf))
        return false;
      if (!BEncodeWriteDictEntry("s", sender, buf))
        return false;
      if (!tag.IsZero())
      {
        if (!BEncodeWriteDictEntry("t", tag, buf))
          return false;
      }
      if (!BEncodeWriteDictInt("v", version, buf))
        return false;
      return bencode_end(buf);
    }

    std::vector<char>
    ProtocolMessage::EncodeAuthInfo() const
    {
      std::array<byte_t, 1024> info;
      llarp_buffer_t buf{info};
      if (not bencode_start_dict(&buf))
        throw std::runtime_error("impossibly small buffer");
      if (not BEncodeWriteDictInt("a", proto, &buf))
        throw std::runtime_error("impossibly small buffer");
      if (not BEncodeWriteDictEntry("i", introReply, &buf))
        throw std::runtime_error("impossibly small buffer");
      if (not BEncodeWriteDictEntry("s", sender, &buf))
        throw std::runtime_error("impossibly small buffer");
      if (not BEncodeWriteDictEntry("t", tag, &buf))
        throw std::runtime_error("impossibly small buffer");
      if (not BEncodeWriteDictInt("v", version, &buf))
        throw std::runtime_error("impossibly small buffer");
      if (not bencode_end(&buf))
        throw std::runtime_error("impossibly small buffer");
      const std::size_t encodedSize = buf.cur - buf.base;
      std::vector<char> data;
      data.resize(encodedSize);
      std::copy_n(buf.base, encodedSize, data.data());
      return data;
    }

    ProtocolFrame::~ProtocolFrame() = default;

    bool
    ProtocolFrame::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      if (!BEncodeWriteDictMsgType(buf, "A", "H"))
        return false;
      if (not IsZero(C))
      {
        if (!BEncodeWriteDictEntry("C", C, buf))
          return false;
      }
      if (D.size() > 0)
      {
        if (!BEncodeWriteDictEntry("D", D, buf))
          return false;
      }
      if (!BEncodeWriteDictEntry("F", F, buf))
        return false;
      if (!N.IsZero())
      {
        if (!BEncodeWriteDictEntry("N", N, buf))
          return false;
      }
      if (R)
      {
        if (!BEncodeWriteDictInt("R", R, buf))
          return false;
      }
      if (!T.IsZero())
      {
        if (!BEncodeWriteDictEntry("T", T, buf))
          return false;
      }
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ProtocolFrame::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      bool read = false;
      if (key.startswith("A"))
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(val, &strbuf))
          return false;
        if (strbuf.sz != 1)
          return false;
        return *strbuf.cur == 'H';
      }
      if (!BEncodeMaybeReadDictEntry("D", D, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("F", F, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("C", C, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("N", N, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictInt("R", R, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("T", T, read, key, val))
        return false;
      if (!BEncodeMaybeVerifyVersion("V", version, llarp::constants::proto_version, read, key, val))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, key, val))
        return false;
      return read;
    }

    bool
    ProtocolFrame::DecryptPayloadInto(
        const SharedSecret& sharedkey, std::vector<ProtocolMessage>& msgs) const
    {
      Encrypted_t tmp = D;
      auto buf = tmp.Buffer();
      TunnelNonce n{};
      MemWipe{&n};
      static_assert(decltype(N)::SIZE == n.size());
      std::copy_n(N.begin(), n.size(), n.begin());
      CryptoManager::instance()->xchacha20(*buf, sharedkey, n);
      if (buf->base[0] == 'd')
      {
        auto& msg = msgs.emplace_back();
        return bencode_decode_dict(msg, buf);
      }
      if (buf->base[0] == 'l')
        return BEncodeReadList(msgs, buf);
      return false;
    }

    bool
    ProtocolFrame::Sign(const Identity& localIdent)
    {
      Z.Zero();
      std::array<byte_t, constants::service_proto_frame_max_size> tmp{};
      llarp_buffer_t buf(tmp);
      // encode
      if (!BEncode(&buf))
      {
        LogError("message too big to encode");
        return false;
      }
      // rewind
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // sign
      return localIdent.Sign(Z, buf);
    }

    bool
    ProtocolFrame::EncryptAndSign(
        const ProtocolMessage& msg, const SharedSecret& sessionKey, const Identity& localIdent)
    {
      std::array<byte_t, constants::service_proto_frame_max_size> tmp{};
      llarp_buffer_t buf(tmp);
      // encode message
      if (!msg.BEncode(&buf))
      {
        LogError("message too big to encode");
        return false;
      }
      // rewind
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // encrypt
      {
        TunnelNonce n{};
        MemWipe{&n};
        static_assert(decltype(N)::SIZE == n.size());
        std::copy_n(N.begin(), n.size(), n.begin());
        CryptoManager::instance()->xchacha20(buf, sessionKey, n);
      }
      // put encrypted buffer
      D = buf;
      // zero out signature
      Z.Zero();
      llarp_buffer_t buf2(tmp);
      // encode frame
      if (!BEncode(&buf2))
      {
        LogError("frame too big to encode");
        DumpBuffer(buf2);
        return false;
      }
      // rewind
      buf2.sz = buf2.cur - buf2.base;
      buf2.cur = buf2.base;
      // sign
      if (!localIdent.Sign(Z, buf2))
      {
        LogError("failed to sign? wtf?!");
        return false;
      }
      return true;
    }

    bool
    ProtocolFrame::EncryptAndSign(
        std::deque<ProtocolMessage>& msgs,
        const SharedSecret& sessionKey,
        const Identity& localIdent)
    {
      {
        std::array<byte_t, constants::service_proto_message_max_size> tmp{};
        llarp_buffer_t buf(tmp);
        if (not bencode_start_list(&buf))
          return false;
        for (size_t idx{}; idx < msgs.size(); ++idx)
        {
          auto& msg = msgs[idx];
          // encode message
          auto* cur = buf.cur;
          if (not msg.BEncode(&buf))
          {
            // too big, stop here.
            buf.cur = cur;
            break;
          }
          msgs.pop_front();
        }
        if (not bencode_end(&buf))
          return false;
        // rewind
        buf.sz = buf.cur - buf.base;
        buf.cur = buf.base;
        // encrypt
        {
          TunnelNonce n{};
          MemWipe{&n};
          static_assert(decltype(N)::SIZE == n.size());
          std::copy_n(N.begin(), n.size(), n.begin());
          CryptoManager::instance()->xchacha20(buf, sessionKey, n);
        }
        // put encrypted buffer
        D = buf;
        // zero out signature
        Z.Zero();
      }
      {
        std::array<uint8_t, constants::service_proto_frame_max_size> tmp{};
        llarp_buffer_t buf(tmp);
        // encode frame
        if (!BEncode(&buf))
        {
          LogError("frame too big to encode");
          DumpBuffer(buf);
          return false;
        }
        // rewind
        buf.sz = buf.cur - buf.base;
        buf.cur = buf.base;
        // sign
        if (!localIdent.Sign(Z, buf))
        {
          LogError("failed to sign? wtf?!");
          return false;
        }
      }
      return true;
    }

    struct AsyncFrameDecrypt
    {
      path::Path_ptr path;
      EventLoop_ptr loop;
      std::vector<ProtocolMessage> msgs;
      const Identity& m_LocalIdentity;
      Endpoint* handler;
      const ProtocolFrame frame;
      const Introduction fromIntro;

      AsyncFrameDecrypt(
          EventLoop_ptr l,
          const Identity& localIdent,
          Endpoint* h,
          const ProtocolFrame& f,
          const Introduction& recvIntro)
          : loop(std::move(l))
          , m_LocalIdentity(localIdent)
          , handler(h)
          , frame(f)
          , fromIntro(recvIntro)
      {}

      static void
      Work(std::shared_ptr<AsyncFrameDecrypt> self)
      {
        auto crypto = CryptoManager::instance();
        SharedSecret K;
        SharedSecret sharedKey;
        // copy
        ProtocolFrame frame(self->frame);
        if (!crypto->pqe_decrypt(self->frame.C, K, pq_keypair_to_secret(self->m_LocalIdentity.pq)))
        {
          LogError("pqke failed C=", self->frame.C);
          return;
        }
        // decrypt
        auto buf = frame.D.Buffer();
        {
          TunnelNonce n{};
          MemWipe{&n};
          static_assert(decltype(self->frame.N)::SIZE == n.size());
          std::copy_n(self->frame.N.begin(), n.size(), n.begin());
          crypto->xchacha20(*buf, K, n);
        }
        if (buf->cur[0] == 'd')
        {
          auto& msg = self->msgs.emplace_back();
          if (not bencode_decode_dict(msg, buf))
            return;
        }
        else if (buf->cur[0] == 'l')
        {
          if (not BEncodeReadList(self->msgs, buf))
            return;
        }
        else
          return;

        const auto& sender = self->msgs[0].sender;
        // verify signature of outer message after we parsed the inner message
        if (!self->frame.Verify(sender))
        {
          LogError(
              "intro frame has invalid signature Z=",
              self->frame.Z,
              " from ",
              sender.Addr().ToString());
          return;
        }
        // KEX
        {
          const auto& msg = self->msgs[0];
          if (self->handler->HasConvoTag(msg.tag))
          {
            LogError("dropping duplicate convo tag T=", msg.tag);
            return;
          }

          // PKE (A, B, N)
          SharedSecret sharedSecret;
          path_dh_func dh_server = util::memFn(&Crypto::dh_server, CryptoManager::instance());

          if (not self->m_LocalIdentity.KeyExchange(dh_server, sharedSecret, sender, self->frame.N))
          {
            LogError("x25519 key exchange failed");
            return;
          }
          std::array<byte_t, 64> tmp{};
          MemWipe{&tmp};
          // K
          std::copy(K.begin(), K.end(), tmp.begin());
          // S = HS( K + PKE( A, B, N))
          std::copy(sharedSecret.begin(), sharedSecret.end(), tmp.begin() + 32);
          {
            ShortHash h{};
            MemWipe{&h};
            static_assert(h.size() == sharedKey.size());
            crypto->shorthash(h, llarp_buffer_t(tmp));
            sharedKey = h.data();
          }
        }

        const PathID_t from = self->frame.F;
        for (auto& msg : self->msgs)
        {
          auto msg_ptr = std::make_shared<ProtocolMessage>(std::move(msg));
          msg_ptr->handler = self->handler;
          self->handler->AsyncProcessAuthMessage(
              msg_ptr,
              [path = self->path,
               msg = msg_ptr,
               from,
               handler = self->handler,
               fromIntro = self->fromIntro,
               sharedKey](AuthResult result) {
                if (result.code == AuthResultCode::eAuthAccepted)
                {
                  if (handler->WantsOutboundSession(msg->sender.Addr()))
                  {
                    handler->PutSenderFor(msg->tag, msg->sender, false);
                  }
                  else
                  {
                    handler->PutSenderFor(msg->tag, msg->sender, true);
                  }
                  handler->PutReplyIntroFor(msg->tag, msg->introReply);
                  handler->PutCachedSessionKeyFor(msg->tag, sharedKey);
                  handler->SendAuthResult(path, from, msg->tag, result);
                  LogInfo("auth okay for T=", msg->tag, " from ", msg->sender.Addr());
                  ProtocolMessage::ProcessAsync(path, from, msg);
                }
                else
                {
                  LogWarn("auth not okay for T=", msg->tag, ": ", result.reason);
                }
              });
        }
      }
    };

    ProtocolFrame&
    ProtocolFrame::operator=(const ProtocolFrame& other)
    {
      C = other.C;
      D = other.D;
      F = other.F;
      N = other.N;
      Z = other.Z;
      T = other.T;
      R = other.R;
      S = other.S;
      version = other.version;
      return *this;
    }

    struct AsyncDecrypt
    {
      ServiceInfo si;
      SharedSecret shared;
      ProtocolFrame frame;
    };

    bool
    ProtocolFrame::AsyncDecryptAndVerify(
        EventLoop_ptr loop,
        path::Path_ptr recvPath,
        const Identity& localIdent,
        Endpoint* handler,
        std::function<void(std::shared_ptr<ProtocolMessage>)> hook) const
    {
      if (T.IsZero())
      {
        // we need to dh
        auto dh =
            std::make_shared<AsyncFrameDecrypt>(loop, localIdent, handler, *this, recvPath->intro);
        dh->path = recvPath;
        handler->Router()->QueueWork([dh = std::move(dh)] { return AsyncFrameDecrypt::Work(dh); });
        return true;
      }

      auto v = std::make_shared<AsyncDecrypt>();

      if (!handler->GetCachedSessionKeyFor(T, v->shared))
      {
        LogError("No cached session for T=", T);
        return false;
      }
      if (v->shared.IsZero())
      {
        LogError("bad cached session key for T=", T);
        return false;
      }

      if (!handler->GetSenderFor(T, v->si))
      {
        LogError("No sender for T=", T);
        return false;
      }
      if (v->si.Addr().IsZero())
      {
        LogError("Bad sender for T=", T);
        return false;
      }

      v->frame = *this;
      auto callback = [loop, hook](std::shared_ptr<ProtocolMessage> msg) {
        if (hook)
        {
          loop->call([msg, hook]() { hook(msg); });
        }
      };
      handler->Router()->QueueWork([v, recvPath = std::move(recvPath), callback, handler]() {
        auto resetTag = [handler, tag = v->frame.T, from = v->frame.F, path = recvPath]() {
          handler->ResetConvoTag(tag, path, from);
        };

        if (not v->frame.Verify(v->si))
        {
          LogError("Signature failure from ", v->si.Addr());
          handler->Loop()->call_soon(resetTag);
          return;
        }
        std::vector<ProtocolMessage> msgs;
        if (not v->frame.DecryptPayloadInto(v->shared, msgs))
        {
          LogError("failed to decrypt message from ", v->si.Addr());
          handler->Loop()->call_soon(resetTag);
          return;
        }
        for (auto& msg : msgs)
        {
          auto msg_ptr = std::make_shared<ProtocolMessage>(std::move(msg));
          msg_ptr->handler = handler;
          msg_ptr->tag = v->frame.T;
          callback(msg_ptr);
          RecvDataEvent ev;
          ev.fromPath = recvPath;
          ev.pathid = v->frame.F;
          ev.msg = std::move(msg_ptr);
          handler->QueueRecvData(std::move(ev));
        }
      });
      return true;
    }

    bool
    ProtocolFrame::operator==(const ProtocolFrame& other) const
    {
      return C == other.C && D == other.D && N == other.N && Z == other.Z && T == other.T
          && S == other.S && version == other.version;
    }

    size_t
    ProtocolFrame::overhead() const noexcept
    {
      return IMessage::overhead() + overhead_for(C) + overhead_for(R) + overhead_for(N)
          + overhead_for(F) + overhead_for(T) + overhead_for(Z);
    }

    bool
    ProtocolFrame::Verify(const ServiceInfo& svc) const
    {
      ProtocolFrame copy(*this);
      // save signature
      // zero out signature for verify
      copy.Z.Zero();
      // serialize
      std::array<byte_t, constants::service_proto_frame_max_size> tmp{};
      llarp_buffer_t buf(tmp);
      if (!copy.BEncode(&buf))
      {
        LogError("bencode fail");
        return false;
      }

      // rewind buffer
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // verify
      return svc.Verify(buf, Z);
    }

    bool
    ProtocolFrame::HandleMessage(routing::IMessageHandler* h, AbstractRouter* /*r*/) const
    {
      return h->HandleHiddenServiceFrame(*this);
    }

  }  // namespace service
}  // namespace llarp
