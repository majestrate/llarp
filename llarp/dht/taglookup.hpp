#pragma once
#include "tx.hpp"
#include <llarp/service/intro_set.hpp>
#include <llarp/service/tag.hpp>

namespace llarp::dht
{
  struct TagLookup : public TX<service::Tag, service::EncryptedIntroSet>
  {
    uint64_t recursionDepth;
    TagLookup(
        const TXOwner& asker, const service::Tag& tag, AbstractContext* ctx, uint64_t recursion)
        : TX<service::Tag, service::EncryptedIntroSet>(asker, tag, ctx), recursionDepth(recursion)
    {}

    bool
    Validate(const service::EncryptedIntroSet& introset) const override;

    void
    Start(const TXOwner& peer) override;

    void
    SendReply() override;
  };
}  // namespace llarp::dht
