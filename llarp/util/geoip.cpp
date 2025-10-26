#include "geoip.hpp"

#include "str.hpp"
#include <llarp/util/logging.hpp>

#ifdef WITH_GEOIP
#include <GeoIP.h>
#else
struct GeoIP
{};
struct GeoIPLookup
{};

#define GEOIP_STANDARD (0)

GeoIP*
GeoIP_new(int)
{
  return nullptr;
}
const char*
GeoIP_country_code_by_addr_gl(GeoIP*, const char*, GeoIPLookup*)
{
  return nullptr;
}

void
GeoIP_delete(GeoIP*)
{}

#endif

namespace llarp::util
{

  static auto logcat = log::Cat("geoip");
  struct GeoIPHelper::Pimpl
  {
    ::GeoIP* geoip_ptr;

    Pimpl()
    {
      geoip_ptr = ::GeoIP_new(GEOIP_STANDARD);
    }

    ~Pimpl()
    {
      ::GeoIP_delete(geoip_ptr);
    }

    static constexpr auto CacheEntryDuration = 30min;

    template <typename Cache_t>
    std::optional<std::string>
    get_country_code(const SockAddr& addr, Cache_t& cache) const
    {
      std::string country_code{};
      if (auto itr = cache.find(addr); itr != cache.end())
        country_code = itr->second.country_code;

      if (not country_code.empty())
        return country_code;

      std::string addr_str = addr.getIPv4().ToString();
      ::GeoIPLookup lookup{};
      const char* code = ::GeoIP_country_code_by_addr_gl(geoip_ptr, addr_str.c_str(), &lookup);
      if (code == nullptr)
      {
        log::info(logcat, "GeoIP lookup for '{}' did not find any entries", addr_str);
        return std::nullopt;
      }
      auto& cache_entry = cache[addr];
      cache_entry.expires_at = std::chrono::steady_clock::now() + CacheEntryDuration;
      cache_entry.country_code = lowercase_ascii_string(code);
      log::info(
          logcat,
          "address '{}' geoip lookup found country='{}'",
          addr_str,
          cache_entry.country_code);
      return cache_entry.country_code;
    }
  };

  GeoIPHelper::GeoIPHelper() : m_Impl{std::make_shared<Pimpl>()}
  {}

  GeoIPHelper::~GeoIPHelper()
  {}

  bool
  GeoIPHelper::address_in_country_code_set(
      const SockAddr& addr, const std::unordered_set<std::string>& country_codes)
  {
    NullLock lock{m_Access};
    if (const auto maybe = m_Impl->get_country_code(addr, m_LookupCache))
      return country_codes.contains(*maybe);
    return false;
  }

  void
  GeoIPHelper::maybe_decay_cache()
  {
    NullLock lock{m_Access};
    const auto now = std::chrono::steady_clock::now();
    auto itr = m_LookupCache.begin();
    while (itr != m_LookupCache.end())
    {
      if (itr->second.expires_at <= now)
        itr = m_LookupCache.erase(itr);
      else
        ++itr;
    }
  }

  std::shared_ptr<GeoIPHelper>
  GeoIPHelperInstance()
  {
    static std::shared_ptr<GeoIPHelper> g_instance{nullptr};
    if (g_instance)
      return g_instance;
    g_instance = std::make_shared<GeoIPHelper>();
    return g_instance;
  }

}  // namespace llarp::util