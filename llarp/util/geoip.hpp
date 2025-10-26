#pragma once
#include <llarp/net/sock_addr.hpp>
#include <llarp/util/thread/threading.hpp>
#include <unordered_set>
#include <unordered_map>

namespace llarp::util
{

  ///
  /// GeoIP helper class.
  ///
  class GeoIPHelper
  {
    struct Pimpl;

    // a cached lookup.
    struct CacheEntry
    {
      std::string country_code;
      std::chrono::time_point<std::chrono::steady_clock> expires_at;
    };

    NullMutex m_Access;
    std::unordered_map<SockAddr, CacheEntry> m_LookupCache;
    std::shared_ptr<Pimpl> m_Impl;

   public:
    GeoIPHelper();
    ~GeoIPHelper();

    /// return true if this address is located in one of the given countries.
    /// caches any internal geoip lookups done.
    bool
    address_in_country_code_set(
        const SockAddr& addr, const std::unordered_set<std::string>& country_codes);

    /// expire cache entries if needed.
    void
    maybe_decay_cache();
  };

  /// get singleton geoip helper instance.
  std::shared_ptr<GeoIPHelper>
  GeoIPHelperInstance();
}  // namespace llarp::util
