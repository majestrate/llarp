#pragma once
#include "platform.hpp"

#include <llarp/constants/platform.hpp>
#include <type_traits>

namespace llarp::dns
{
  namespace apple
  {
    /// a dns platform that sets dns on macOS
    class Platform : public I_Platform
    {
     public:
      virtual ~Platform() = default;

      void
      set_resolver(unsigned int if_index, llarp::SockAddr dns, bool global) override;
    };
  }  // namespace apple
  using Apple_Platform_t =
      std::conditional_t<llarp::platform::is_apple, apple::Platform, Null_Platform>;
}  // namespace llarp::dns
