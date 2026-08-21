#pragma once
#include "types.hpp"
#include "buffer.hpp"
#include <fmt/ranges.h>
#include <llarp/util/formattable.hpp>

#include <oxenc/hex.h>

#include <array>
#include <cstddef>
#include <memory>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <compare>

extern "C"
{
  extern void
  randombytes(unsigned char* const ptr, unsigned long long sz);

  extern int
  sodium_is_zero(const unsigned char* n, const size_t nlen);
}

bool
bencode_write_bytestring(llarp_buffer_t*, const void*, size_t);
bool
bencode_read_string(llarp_buffer_t*, llarp_buffer_t*);

namespace llarp::service
{
  struct ConvoTag;
}

namespace llarp
{

  void
  Zero(void*, size_t);

  /// Assigns a value from a bare pointer to a fixzed size container.
  template <typename Container_t>
  struct AssignPtr
  {
    Container_t& ref;
    Container_t&
    operator=(const byte_t* ptr)
    {
      std::memcpy(ref.data(), ptr, ref.size());
      return ref;
    }
  };

  template <size_t sz>
  using AlignedBuffer = std::array<byte_t, sz>;

  // True if T implements aligned buffer style interface.
  template <typename T>
  constexpr inline bool is_aligned_buffer = false;

  template <typename T>
  constexpr inline bool is_std_array = false;

  template <size_t sz>
  constexpr inline bool is_std_array<std::array<uint8_t, sz>> = true;

  template <typename T, typename = void>
  struct aligned_buffer_size;

  template <typename T>
    requires(is_aligned_buffer<T> and not is_std_array<T>)
  struct aligned_buffer_size<T>
  {
    static constexpr size_t value = T::SIZE;
  };

  template <typename T>
    requires is_std_array<T>
  struct aligned_buffer_size<T>
  {
    static constexpr size_t value = std::tuple_size_v<T>;
  };

  template <typename T>
  inline constexpr size_t aligned_buffer_size_v = aligned_buffer_size<T>::value;

  template <typename T>
    requires is_aligned_buffer<T>
  void
  Randomize(T& t)
  {
    ::randombytes(t.data(), t.size());
    if constexpr (std::is_same_v<T, service::ConvoTag>)
      t.data()[0] = 0xfc;
  }

  template <typename T>
    requires is_aligned_buffer<T> or is_std_array<T>
  size_t
  overhead_for(const T& t) noexcept
  {
    if (::sodium_is_zero(t.data(), t.size()))
      return 0;
    // bencode overhead.
    return std::floor(std::log10(t.size())) + 1 + 1 + t.size();
  }

  template <typename Int_t>
    requires std::is_integral_v<Int_t> and std::is_unsigned_v<Int_t>
  size_t
  overhead_for(const Int_t& i) noexcept
  {
    // bencode overhead
    if (i == 0)
      return 1 + 2;
    return std::floor(std::log10(i)) + 1 + 2;
  }

  namespace routing
  {
    struct IMessage;
  }

  template <typename Msg_t>
    requires std::is_base_of_v<routing::IMessage, Msg_t>
  size_t
  overhead_for(const Msg_t& msg) noexcept
  {
    return msg.overhead();
  }

}  // namespace llarp

namespace fmt
{
  /// disable range formatting for aligned buffer.
  template <typename T>
  struct range_format_kind<T, char, std::enable_if_t<llarp::is_aligned_buffer<T>>>
  {
    static constexpr auto value = range_format::disabled;
  };

  // Any aligned buffer like gets hex formatted when output:
  template <typename T>
  struct formatter<
      T,
      char,
      std::enable_if_t<llarp::is_aligned_buffer<T> and not llarp::IsToStringFormattable<T>>>
      : formatter<std::string_view>
  {
    template <typename FormatContext>
    auto
    format(const T& val, FormatContext& ctx) const
    {
      auto it = oxenc::hex_encoder{val.begin(), val.end()};
      return std::copy(it, it.end(), ctx.out());
    }
  };
}  // namespace fmt

namespace std
{
  template <size_t sz>
  struct hash<array<uint8_t, sz>>
  {
    static_assert(sz >= sizeof(size_t));

    std::size_t
    operator()(const array<uint8_t, sz>& buf) const noexcept
    {
      std::size_t h{};
      std::memcpy(&h, buf.data(), sizeof(std::size_t));
      return h;
    }
  };
}  // namespace std

#define ALIGNED_BUFFER_MEMBERS_NO_OPERS(_Kind_t, _sz)                                  \
 public:                                                                               \
  static constexpr size_t SIZE = _sz;                                                  \
  constexpr size_t size() const noexcept                                               \
  {                                                                                    \
    return SIZE;                                                                       \
  }                                                                                    \
  using Data = std::array<byte_t, SIZE>;                                               \
  constexpr byte_t* data() noexcept                                                    \
  {                                                                                    \
    return m_data.data();                                                              \
  }                                                                                    \
  constexpr const byte_t* data() const noexcept                                        \
  {                                                                                    \
    return m_data.data();                                                              \
  }                                                                                    \
  constexpr const std::array<byte_t, SIZE>& as_array() const noexcept                  \
  {                                                                                    \
    return m_data;                                                                     \
  }                                                                                    \
  using iter_t = Data::iterator;                                                       \
  using c_iter_t = Data::const_iterator;                                               \
  constexpr iter_t begin() noexcept                                                    \
  {                                                                                    \
    return m_data.begin();                                                             \
  }                                                                                    \
  constexpr iter_t end() noexcept                                                      \
  {                                                                                    \
    return m_data.end();                                                               \
  }                                                                                    \
  constexpr c_iter_t begin() const noexcept                                            \
  {                                                                                    \
    return m_data.begin();                                                             \
  }                                                                                    \
  constexpr c_iter_t end() const noexcept                                              \
  {                                                                                    \
    return m_data.end();                                                               \
  }                                                                                    \
  std::string_view ToView() const noexcept                                             \
  {                                                                                    \
    return std::string_view{                                                           \
        reinterpret_cast<const char*>(begin()), reinterpret_cast<const char*>(end())}; \
  }                                                                                    \
  void Fill(byte_t ch) noexcept                                                        \
  {                                                                                    \
    m_data.fill(ch);                                                                   \
  }                                                                                    \
                                                                                       \
 private:                                                                              \
  Data m_data{};

#define ALIGNED_BUFFER_MEMBERS_NO_CTOR(_Kind_t, _sz)                              \
  ALIGNED_BUFFER_MEMBERS_NO_OPERS(_Kind_t, _sz)                                   \
 public:                                                                          \
  constexpr bool operator==(const _Kind_t& other) const noexcept                  \
  {                                                                               \
    return as_array() == other.as_array();                                        \
  }                                                                               \
  constexpr bool operator!=(const _Kind_t& other) const noexcept                  \
  {                                                                               \
    return not(*this == other);                                                   \
  }                                                                               \
  constexpr bool operator<(const _Kind_t& other) const noexcept                   \
  {                                                                               \
    return as_array() < other.as_array();                                         \
  }                                                                               \
  _Kind_t& operator=(const byte_t* ptr) noexcept                                  \
  {                                                                               \
    return AssignPtr{*this} = ptr;                                                \
  }                                                                               \
  template <typename Other_t>                                                     \
    requires llarp::is_aligned_buffer<Other_t>                                    \
  constexpr _Kind_t operator^(const Other_t& other) const noexcept                \
  {                                                                               \
    static_assert(llarp::aligned_buffer_size_v<Other_t> == SIZE);                 \
    _Kind_t ret{};                                                                \
    std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>{}); \
    return ret;                                                                   \
  }

#define ALIGNED_BUFFER_MEMBERS_NO_SERIALIZE(_Kind_t, _sz) \
  ALIGNED_BUFFER_MEMBERS_NO_CTOR(_Kind_t, _sz)            \
 public:                                                  \
  _Kind_t() = default;                                    \
  explicit _Kind_t(const byte_t* buf)                     \
  {                                                       \
    *this = buf;                                          \
  }                                                       \
  template <typename Kind_t>                              \
    requires llarp::is_aligned_buffer<Kind_t>             \
  explicit _Kind_t(const Kind_t& data)                    \
  {                                                       \
    static_assert(llarp::aligned_buffer_size_v<Kind_t> == SIZE); \
    std::copy_n(data.begin(), size(), m_data.begin());    \
  }

#define ALIGNED_BUFFER_MEMBERS(_kind_t, _sz)              \
  ALIGNED_BUFFER_MEMBERS_NO_SERIALIZE(_kind_t, _sz)       \
 public:                                                  \
  inline std::string ToHex() const noexcept               \
  {                                                       \
    return oxenc::to_hex(begin(), end());                 \
  }                                                       \
  inline bool BEncode(llarp_buffer_t* buf) const          \
  {                                                       \
    return bencode_write_bytestring(buf, data(), size()); \
  }                                                       \
  inline bool BDecode(llarp_buffer_t* buf)                \
  {                                                       \
    llarp_buffer_t str{};                                 \
    if (not bencode_read_string(buf, &str))               \
      return false;                                       \
    if (str.sz != size())                                 \
      return false;                                       \
    std::copy_n(str.base, size(), begin());               \
    return true;                                          \
  }                                                       \
  inline void Zero()                                      \
  {                                                       \
    llarp::Zero(data(), size());                          \
  }                                                       \
  inline bool IsZero() const                              \
  {                                                       \
    return ::sodium_is_zero(data(), size());              \
  }
