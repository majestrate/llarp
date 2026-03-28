#pragma once

#include <llarp/constants/proto.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/util/buffer.hpp>

namespace llarp
{
  struct AbstractRouter;
  namespace routing
  {
    struct IMessageHandler;

    struct IMessage
    {
      PathID_t from;
      uint64_t S{0};
      uint64_t version = llarp::constants::proto_version;

      IMessage() = default;

      virtual ~IMessage() = default;

      virtual bool
      BEncode(llarp_buffer_t* buf) const = 0;

      virtual bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) = 0;

      virtual bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const = 0;

      virtual void
      Clear() = 0;

      bool
      operator<(const IMessage& other) const
      {
        return S < other.S;
      }

      virtual size_t
      overhead() const noexcept;

      size_t
      total_size() const noexcept;
    };
  }  // namespace routing

  template <typename T>
    requires std::is_base_of_v<routing::IMessage, T>
  size_t
  total_size_for(const T& t)
  {
    return t.total_size();
  }
}  // namespace llarp
