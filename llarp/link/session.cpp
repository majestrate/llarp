#include <llarp/util/alloc.h>
#include "session.hpp"

namespace llarp
{
  bool
  ILinkSession::IsRelay() const
  {
    return GetRemoteRC().IsPublicRouter();
  }

}  // namespace llarp
