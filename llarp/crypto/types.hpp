#pragma once

#include "constants.hpp"
#include <llarp/router_id.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/types.hpp>
#include <llarp/util/formattable.hpp>
#include <sodium/utils.h>
#include <llarp/util/mem.hpp>

namespace llarp
{
  template <typename T, size_t sz = sizeof(T)>
  class MemWipe
  {
    void* m_ref{nullptr};

   public:
    explicit MemWipe(T* ref) : m_ref{reinterpret_cast<void*>(ref)}
    {}
    explicit MemWipe(void* ref) : m_ref{ref}
    {}

    MemWipe(MemWipe&&) = delete;

    MemWipe(const MemWipe&) = delete;

    ~MemWipe()
    {
      if (m_ref)
        ::sodium_memzero(m_ref, sz);
    }

    bool
    IsZero() const;
  };
}  // namespace llarp

namespace llarp
{

  template <size_t sz>
  struct WipeArray
  {
    ALIGNED_BUFFER_MEMBERS(WipeArray, sz)
   public:
    ~WipeArray()
    {
      ::sodium_memzero(data(), size());
    }
  };

  template <>
  constexpr inline bool is_aligned_buffer<WipeArray<32>> = true;

  using SharedSecret = WipeArray<SHAREDKEYSIZE>;
  using KeyExchangeNonce = WipeArray<32>;

  struct PubKey final
  {
    ALIGNED_BUFFER_MEMBERS(PubKey, PUBKEYSIZE)

    std::string
    ToString() const;

    bool
    FromString(const std::string& str);

    operator RouterID() const
    {
      return RouterID{data()};
    }

    bool
    operator==(const RouterID& rid) const
    {
      return as_array() == rid.as_array();
    }
  };

  inline auto
  operator<=>(const PubKey& lhs, const PubKey& rhs)
  {
    return lhs.as_array() <=> rhs.as_array();
  }

  inline auto
  operator<=>(const PubKey& lhs, const RouterID& rhs)
  {
    return lhs.as_array() <=> rhs.as_array();
  }

  inline auto
  operator<=>(const RouterID& lhs, const PubKey& rhs)
  {
    return lhs.as_array() <=> rhs.as_array();
  }

  struct PrivateKey;

  /// Stores a sodium "secret key" value, which is actually the seed
  /// concatenated with the public key.  Note that the seed is *not* the private
  /// key value itself, but rather the seed from which it can be calculated.
  struct SecretKey final
  {
    ALIGNED_BUFFER_MEMBERS(SecretKey, SECKEYSIZE)
   public:
    ~SecretKey();
    // The full data
    explicit SecretKey(const std::array<byte_t, SECKEYSIZE>& buf);

    // Just the seed, we recalculate the pubkey
    explicit SecretKey(const std::array<byte_t, SEEDSIZE>& seed);

    /// recalculate public component
    bool
    Recalculate();

    std::string_view
    ToString() const
    {
      return "[secretkey]";
    }

    PubKey
    toPublic() const
    {
      return PubKey(data() + 32);
    }

    /// Computes the private key from the secret key (which is actually the
    /// seed)
    bool
    toPrivate(PrivateKey& key) const;

    template <typename fspath_t>
    bool
    LoadFromFile(const fspath_t& fname);

    template <typename fspath_t>
    bool
    SaveToFile(const fspath_t& fname) const;
  };

  /// PrivateKey is similar to SecretKey except that it only stores the private
  /// key value and a hash, unlike SecretKey which stores the seed from which
  /// the private key and hash value are generated.  This is primarily intended
  /// for use with derived keys, where we can derive the private key but not the
  /// seed.
  struct PrivateKey final
  {
    ALIGNED_BUFFER_MEMBERS(PrivateKey, 64)
   public:
    ~PrivateKey();

    explicit PrivateKey(const AlignedBuffer<SIZE>& key_and_hash);

    /// Returns a pointer to the beginning of the 32-byte hash which is used for
    /// pseudorandomness when signing with this private key.
    constexpr const byte_t*
    signingHash() const
    {
      return data() + 32;
    }

    /// Returns a pointer to the beginning of the 32-byte hash which is used for
    /// pseudorandomness when signing with this private key.
    constexpr byte_t*
    signingHash()
    {
      return data() + 32;
    }

    std::string_view
    ToString() const
    {
      return "[privatekey]";
    }

    /// Computes the public key
    bool
    toPublic(PubKey& pubkey) const;
  };

  /// IdentitySecret is a secret key from a service node secret seed
  struct IdentitySecret final
  {
    ALIGNED_BUFFER_MEMBERS_NO_CTOR(IdentitySecret, 32)

   public:
    IdentitySecret() = default;
    ~IdentitySecret();

    /// no copy constructor
    explicit IdentitySecret(const IdentitySecret&) = delete;
    // no byte data constructor
    explicit IdentitySecret(const byte_t*) = delete;

    /// load service node seed from file
    template <typename fspath_t>
    bool
    LoadFromFile(const fspath_t& fname);

    std::string_view
    ToString() const
    {
      return "[IdentitySecret]";
    }
  };

  template <>
  constexpr inline bool IsToStringFormattable<PubKey> = true;
  template <>
  constexpr inline bool IsToStringFormattable<SecretKey> = true;
  template <>
  constexpr inline bool IsToStringFormattable<PrivateKey> = true;
  template <>
  constexpr inline bool IsToStringFormattable<IdentitySecret> = true;

  using ShortHash = std::array<byte_t, SHORTHASHSIZE>;
  using LongHash = std::array<byte_t, HASHSIZE>;

  struct Signature final
  {
    ALIGNED_BUFFER_MEMBERS(Signature, SIGSIZE);

   public:
    byte_t*
    Hi();

    const byte_t*
    Hi() const;

    byte_t*
    Lo();

    const byte_t*
    Lo() const;

    bool
    FromBytestring(llarp_buffer_t* buf)
    {
      if (buf->sz != size())
        return false;
      std::copy_n(buf->base, size(), data());
      return true;
    }
  };

  using TunnelNonce = AlignedBuffer<TUNNONCESIZE>;
  using SymmNonce = AlignedBuffer<NONCESIZE>;
  using SymmKey = AlignedBuffer<32>;

  using PQCipherBlock = AlignedBuffer<PQ_CIPHERTEXTSIZE + 1>;
  using PQPubKey = AlignedBuffer<PQ_PUBKEYSIZE>;

  struct PQKeyPair final
  {
    ALIGNED_BUFFER_MEMBERS(PQKeyPair, PQ_KEYPAIRSIZE)
   public:
    ~PQKeyPair();
  };

  template <>
  constexpr inline bool is_aligned_buffer<std::array<uint8_t, 16>> = true;
  template <>
  constexpr inline bool is_aligned_buffer<std::array<uint8_t, 24>> = true;

  template <>
  constexpr inline bool is_aligned_buffer<std::array<uint8_t, 32>> = true;

  template <>
  constexpr inline bool is_aligned_buffer<std::array<uint8_t, 64>> = true;
  template <>
  constexpr inline bool is_aligned_buffer<Signature> = true;

  template <>
  constexpr inline bool is_aligned_buffer<PQCipherBlock> = true;

  template <>
  constexpr inline bool is_aligned_buffer<PQPubKey> = true;

  template <>
  constexpr inline bool is_aligned_buffer<PubKey> = true;

  /// PKE(result, publickey, secretkey, nonce)
  using path_dh_func =
      std::function<bool(SharedSecret&, const PubKey&, const SecretKey&, const KeyExchangeNonce&)>;

  /// TKE(result, publickey, secretkey, nonce)
  using transport_dh_func =
      std::function<bool(SharedSecret&, const PubKey&, const SecretKey&, const TunnelNonce&)>;

  /// SH(result, body)
  using shorthash_func = std::function<bool(ShortHash&, const llarp_buffer_t&)>;
}  // namespace llarp

namespace std
{
  template <>
  struct hash<llarp::PubKey>
  {
    hash<array<byte_t, llarp::PubKey::SIZE>> m_hasher;
    size_t
    operator()(const llarp::PubKey& pk) const noexcept
    {
      return m_hasher(pk.as_array());
    }
  };
};  // namespace std

namespace llarp
{
  constexpr auto&
  operator^=(std::array<uint8_t, 32>& lhs, const std::array<uint8_t, 32>& rhs)
  {
    transform(lhs.begin(), lhs.end(), rhs.begin(), lhs.begin(), std::bit_xor<>{});
    return lhs;
  }

  constexpr std::array<uint8_t, 32>
  operator^(const std::array<uint8_t, 32>& lhs, const std::array<uint8_t, 32>& rhs)
  {
    std::array<uint8_t, 32> result{};
    std::transform(lhs.begin(), lhs.end(), rhs.begin(), result.begin(), std::bit_xor<>{});
    return result;
  }

};  // namespace llarp
