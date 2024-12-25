
#include "platform.hpp"

#ifdef __linux__
#ifdef ANDROID
#include "android.hpp"
#else
#include "linux.hpp"
#endif
#endif

#include <exception>

namespace llarp::vpn
{
  const llarp::net::Platform*
  IRouteManager::Net_ptr() const
  {
    return llarp::net::Platform::Default_ptr();
  }

  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx)
  {
    (void)ctx;
    std::shared_ptr<Platform> plat;
#ifdef __linux__
#ifdef ANDROID
    plat = std::make_shared<vpn::AndroidPlatform>(ctx);
#else
    plat = std::make_shared<vpn::LinuxPlatform>();
#endif
#endif
    return plat;
  }

}  // namespace llarp::vpn
