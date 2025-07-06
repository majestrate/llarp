#include <llarp/util/alloc.h>
#include "endpoint_state.hpp"

#include <llarp/exit/session.hpp>
#include "endpoint.hpp"
#include "outbound_context.hpp"
#include <llarp/util/str.hpp>

namespace llarp
{
  namespace service
  {
    bool
    EndpointState::Configure(const NetworkConfig& conf)
    {
      if (conf.m_keyfile.has_value())
        m_Keyfile = conf.m_keyfile->string();
      m_SnodeBlacklist = conf.m_snodeBlacklist;
      m_ExitEnabled = conf.m_AllowExit;

      for (const auto& record : conf.m_SRVRecords)
      {
        m_IntroSet.SRVs.push_back(record.toTuple());
      }

      return true;
    }
  }  // namespace service
}  // namespace llarp
