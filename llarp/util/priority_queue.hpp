#pragma once

#include <queue>
#include <vector>

namespace llarp::util
{
  /// priority queue that sorts in descending order, basically a normal priority queue in reverse.
  template <typename T, typename Container = std::vector<T>>
  using descending_priority_queue =
      std::priority_queue<T, Container, std::greater<typename Container::value_type>>;

  template <
      typename T,
      typename Visit_t,
      typename Queue_t = std::priority_queue<T>,
      typename Container_t = std::vector<T>>
  inline auto
  with_inplace_priority_queue(
      Container_t& vec,
      Visit_t&& visit,
      const typename Queue_t::value_compare& compare = typename Queue_t::value_compare{})
  {
    struct Wrapper
    {
      using container_type = Container_t;
      using value_type = typename container_type::value_type;
      using reference = typename container_type::reference;
      using const_reference = typename container_type::const_reference;
      using size_type = typename container_type::size_type;

      container_type& vector;

      void
      emplace_back(value_type&& value)
      {
        vector.emplace_back(std::move(value));
      }

      auto
      begin()
      {
        return vector.begin();
      }
      auto
      end()
      {
        return vector.end();
      }

      /// the following are to appease the compiler.

      size_type
      size() const
      {
        return vector.size();
      }
      reference
      front()
      {
        return vector.front();
      }
      const_reference
      front() const
      {
        return vector.front();
      }
    };

    const Wrapper wrapper{vec};
    using inner_queue_t =
        std::priority_queue<typename Queue_t::value_type, Wrapper, typename Queue_t::value_compare>;
    inner_queue_t queue{compare, wrapper};
    return visit(static_cast<inner_queue_t&>(queue));
  }

}  // namespace llarp::util
