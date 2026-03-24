#include <llarp/util/alloc.h>
#include "taglookup.hpp"

#include "context.hpp"
#include <llarp/dht/messages/gotintro.hpp>

namespace llarp
{
  namespace dht
  {
    bool
    TagLookup::Validate(const service::EncryptedIntroSet& introset) const
    {
      if (!introset.Verify(parent->Now()))
      {
        llarp::LogWarn("got invalid introset from tag lookup");
        return false;
      }
      if (not introset.topic)
        return false;
      if (*introset.topic != target)
      {
        llarp::LogWarn("got introset with mismatched topic in tag lookup");
        return false;
      }
      return true;
    }

    void
    TagLookup::Start(const TXOwner& peer)
    {
      parent->DHTSendTo(peer.node.Router(), new FindIntroMessage(target, peer.txid));
    }

    void
    TagLookup::SendReply()
    {
      parent->DHTSendTo(whoasked.node.Router(), new GotIntroMessage({}, whoasked.txid));
    }
  }  // namespace dht
}  // namespace llarp
