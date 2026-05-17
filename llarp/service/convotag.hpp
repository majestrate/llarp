#pragma once

#include <llarp/util/aligned.hpp>
#include <llarp/net/net_int.hpp>

#include <llarp/net/net.hpp>

namespace llarp::service
{
  struct ConvoTag final
  {
    ALIGNED_BUFFER_MEMBERS(ConvoTag, 16)
   public:
    sockaddr_in6
    ToV6() const;

    void
    FromV6(sockaddr_in6 saddr);
  };

}  // namespace llarp::service

namespace llarp
{

  template <>
  constexpr inline bool is_aligned_buffer<service::ConvoTag> = true;
}

namespace std
{
  template <>
  struct hash<llarp::service::ConvoTag>
  {
    size_t
    operator()(const llarp::service::ConvoTag& tag) const
    {
      std::hash<std::string_view> h{};
      return h(std::string_view{reinterpret_cast<const char*>(tag.data()), tag.size()});
    }
  };
}  // namespace std
