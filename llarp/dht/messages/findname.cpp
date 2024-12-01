#include "findname.hpp"
#include <oxenc/bt_serialize.h>
#include <llarp/dht/context.hpp>
#include "gotname.hpp"
#include <llarp/router/abstractrouter.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/routing/dht_message.hpp>

namespace llarp::dht
{
  FindNameMessage::FindNameMessage(const Key_t& from, Key_t namehash, uint64_t txid)
      : IMessage(from), NameHash(std::move(namehash)), TxID(txid)
  {}

  bool
  FindNameMessage::BEncode(llarp_buffer_t* buf) const
  {
    const auto data = oxenc::bt_serialize(oxenc::bt_dict{
        {"A", "N"sv},
        {"H", std::string_view{(char*)NameHash.data(), NameHash.size()}},
        {"T", TxID}});
    return buf->write(data.begin(), data.end());
  }

  bool
  FindNameMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key.startswith("H"))
    {
      return NameHash.BDecode(val);
    }
    if (key.startswith("T"))
    {
      return bencode_read_integer(val, &TxID);
    }
    return bencode_discard(val);
  }

  bool
  FindNameMessage::HandleMessage(struct llarp_dht_context* dht, std::vector<Ptr_t>& replies) const
  {
    auto r = dht->impl->GetRouter();
    if (pathID.IsZero() or not r->IsServiceNode())
      return false;

    // opennet case.
    replies.emplace_back(new GotNameMessage(dht::Key_t{}, TxID, service::EncryptedName{}));
    return true;
  }

}  // namespace llarp::dht
