#ifndef LLARP_DHT_TX
#define LLARP_DHT_TX

#include "key.hpp"
#include "txowner.hpp"
#include <llarp/util/logging.hpp>

#include <set>
#include <vector>

namespace llarp
{
  namespace dht
  {
    struct AbstractContext;

    template <typename K, typename V>
    struct TX
    {
      K target;
      AbstractContext* parent;
      std::set<Key_t> peersAsked;
      std::vector<V> valuesFound;
      TXOwner whoasked;

      TX(const TXOwner& asker, const K& k, AbstractContext* p)
          : target(k), parent(p), whoasked(asker)
      {}

      virtual ~TX() = default;

      void
      OnFound(const Key_t& askedPeer, const V& value);

      virtual bool
      Validate(const V& value) const = 0;

      virtual void
      Start(const TXOwner& peer) = 0;

      virtual void
      SendReply() = 0;
    };

    template <typename K, typename V>
    inline void
    TX<K, V>::OnFound(const Key_t& askedPeer, const V& value)
    {
      peersAsked.insert(askedPeer);
      if (Validate(value))
      {
        valuesFound.push_back(value);
      }
    }
  }  // namespace dht
}  // namespace llarp

#endif
