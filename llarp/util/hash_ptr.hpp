#pragma once
#include <functional>

namespace llarp::util
{
  template <typename Ptr_t>
  struct PtrHash
  {
    std::hash<typename Ptr_t::element_type> hasher{};

    size_t
    operator()(const Ptr_t& ptr) const
    {
      if (ptr == nullptr)
        return 0;
      return hasher(*ptr);
    }
  };
}  // namespace llarp::util
