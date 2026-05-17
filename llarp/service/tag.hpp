#pragma once

#include <llarp/dht/key.hpp>
#include <llarp/util/aligned.hpp>
#include <sodium/crypto_generichash.h>

namespace llarp::service
{
  struct Tag
  {
    ALIGNED_BUFFER_MEMBERS(Tag, 16)
   public:
    Tag(const std::string& str) : Tag{}
    {
      // evidently, does nothing on LP64 systems (where size_t is *already*
      // unsigned long but zero-extends this on LLP64 systems
      // 2Jan19: reeee someone undid the patch
      std::copy(
          str.begin(), str.begin() + std::min(std::string::size_type(16), str.size()), begin());
    }

    Tag&
    operator=(const std::string& str)
    {
      std::copy(
          str.begin(), str.begin() + std::min(std::string::size_type(16), str.size()), begin());
      return *this;
    }

    std::string
    ToString() const;

    bool
    Empty() const
    {
      return data()[0] == 0;
    }
  };
}  // namespace llarp::service

namespace std
{
  template <>
  struct hash<llarp::service::Tag>
  {
    hash<llarp::AlignedBuffer<llarp::service::Tag::SIZE>> m_hasher;
    size_t
    operator()(const llarp::service::Tag& tag) const noexcept
    {
      return m_hasher(tag.as_array());
    }
  };
}  // namespace std
