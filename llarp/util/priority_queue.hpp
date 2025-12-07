#pragma once

#include <queue>
#include <vector>

namespace llarp::util
{
  /// priority queue that sorts in descending order, basically a normal priority queue in reverse.
  template <typename T, typename Container = std::vector<T>>
  using descending_priority_queue =
      std::priority_queue<T, Container, std::greater<typename Container::value_type>>;

}  // namespace llarp::util
