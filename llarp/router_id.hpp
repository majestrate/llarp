#pragma once

#include <llarp/util/aligned.hpp>

namespace llarp
{
  struct PubKey;

  struct RouterID
  {
    ALIGNED_BUFFER_MEMBERS(RouterID, 32)
   public:
    std::string
    ToString() const;

    std::string
    ShortString() const;

    bool
    FromString(std::string_view str);

    bool
    operator==(const PubKey&) const;
  };

  template <>
  constexpr inline bool is_aligned_buffer<RouterID> = true;

  template <>
  constexpr inline bool IsToStringFormattable<RouterID> = true;

}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::RouterID>
  {
    hash<array<uint8_t, llarp::RouterID::SIZE>> m_hasher;
    size_t
    operator()(const llarp::RouterID& id) const noexcept
    {
      return m_hasher(id.as_array());
    }
  };
}  // namespace std
