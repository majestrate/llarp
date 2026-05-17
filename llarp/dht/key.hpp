#pragma once

#include <llarp/util/aligned.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/formattable.hpp>

#include <array>

namespace llarp::dht
{
  struct Key_t;

}

namespace llarp
{
  template <>
  constexpr inline bool is_aligned_buffer<dht::Key_t> = true;
}

namespace llarp::dht
{

  struct Key_t
  {
    static_assert(is_aligned_buffer<Key_t>);
    ALIGNED_BUFFER_MEMBERS(Key_t, 32)
   public:
    /// get snode address string
    std::string
    SNode() const
    {
      const RouterID rid{data()};
      return rid.ToString();
    }

    std::string
    ToString() const
    {
      return SNode();
    }
    /*
          template <typename Kind_t>
          requires is_aligned_buffer<Kind_t>
          Key_t
          constexpr operator^(const Kind_t & other) const
          {
            static_assert(other.size() == SIZE);
            Key_t dist{};
            std::transform(begin(), end(), other.begin(), dist.begin(), std::bit_xor<byte_t>());
            return dist;
          }
          */

    auto
    operator<=>(const Key_t& other) const
    {
      return as_array() <=> other.as_array();
    }

    RouterID
    Router() const
    {
      return RouterID{data()};
    }

    template <typename Kind_t>
      requires is_aligned_buffer<Kind_t>
    bool
    operator==(const Kind_t& other) const
    {
      static_assert(other.size() == size());
      return as_array() == other.as_array();
    }
  };
}  // namespace llarp::dht
namespace llarp
{
  template <>
  inline constexpr bool IsToStringFormattable<dht::Key_t> = true;
}  // namespace llarp
