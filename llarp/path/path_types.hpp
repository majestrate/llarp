#pragma once

#include <llarp/crypto/constants.hpp>
#include <llarp/util/aligned.hpp>

namespace llarp
{
  struct PathID_t final
  {
    ALIGNED_BUFFER_MEMBERS(PathID_t, 16)
  };

  template <>
  constexpr inline bool is_aligned_buffer<PathID_t> = true;
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::PathID_t>
  {
    hash<llarp::AlignedBuffer<llarp::PathID_t::SIZE>> m_hasher;
    size_t
    operator()(const llarp::PathID_t& id) const noexcept
    {
      return m_hasher(id.as_array());
    }
  };
}  // namespace std
