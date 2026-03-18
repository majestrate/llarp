#include <llarp/util/alloc.h>
#include "ihophandler.hpp"
#include <llarp/router/abstractrouter.hpp>

namespace llarp::path
{
  // handle data in upstream direction
  bool
  IHopHandler::HandleUpstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*)
  {
    auto& pkt = m_UpstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    return true;
  }

  // handle data in downstream direction
  bool
  IHopHandler::HandleDownstream(const llarp_buffer_t& X, const TunnelNonce& Y, AbstractRouter*)
  {
    auto& pkt = m_DownstreamQueue.emplace_back();
    pkt.first.resize(X.sz);
    std::copy_n(X.base, X.sz, pkt.first.begin());
    pkt.second = Y;
    return true;
  }

}  // namespace llarp::path
