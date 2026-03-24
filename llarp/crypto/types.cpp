#include <llarp/util/alloc.h>
#include "types.hpp"
#include <llarp/util/logging.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/file.hpp>
#include <llarp/util/fs.hpp>

#include <iterator>

#include <oxenc/hex.h>

#include <sodium/crypto_sign.h>
#include <sodium/crypto_sign_ed25519.h>
#include <sodium/crypto_scalarmult_ed25519.h>

namespace llarp
{
  namespace
  {
    auto logcat = log::Cat("cryptography");
  }

  bool
  PubKey::FromString(const std::string& str)
  {
    if (str.size() != 2 * size())
      return false;
    oxenc::from_hex(str.begin(), str.end(), begin());
    return true;
  }

  std::string
  PubKey::ToString() const
  {
    return oxenc::to_hex(begin(), end());
  }

  SecretKey::SecretKey(const std::array<byte_t, SECKEYSIZE>& buf) : SecretKey{}
  {
    std::copy_n(buf.begin(), size(), begin());
  }
  SecretKey::SecretKey(const std::array<byte_t, SEEDSIZE>& seed) : SecretKey{}
  {
    std::copy_n(seed.begin(), SEEDSIZE, begin());
    Recalculate();
  }

  SecretKey::~SecretKey()
  {
    ::sodium_memzero(data(), size());
  }

  template <>
  bool
  SecretKey::LoadFromFile(const fs::path& fname)
  {
    size_t sz;
    std::array<byte_t, 128> tmp{};
    try
    {
      sz = util::slurp_file(fname, tmp.data(), tmp.size());
    }
    catch (const std::exception&)
    {
      return false;
    }

    if (sz == size())
    {
      // is raw buffer
      std::copy_n(tmp.begin(), sz, begin());
      return true;
    }

    llarp_buffer_t buf(tmp);
    llarp_buffer_t str{};
    if (not bencode_read_string(&buf, &str))
      return false;
    if (str.sz != size())
      return false;
    std::copy_n(str.begin(), str.sz, begin());
    return true;
  }

  bool
  SecretKey::Recalculate()
  {
    PrivateKey key;
    PubKey pubkey;
    if (!toPrivate(key) || !key.toPublic(pubkey))
      return false;
    std::memcpy(data() + 32, pubkey.data(), 32);
    return true;
  }

  bool
  SecretKey::toPrivate(PrivateKey& key) const
  {
    // Ed25519 calculates a 512-bit hash from the seed; the first half (clamped)
    // is the private key; the second half is the hash that gets used in
    // signing.
    unsigned char h[crypto_hash_sha512_BYTES];
    if (crypto_hash_sha512(h, data(), 32) < 0)
      return false;
    h[0] &= 248;
    h[31] &= 63;
    h[31] |= 64;
    std::memcpy(key.data(), h, 64);
    return true;
  }

  bool
  PrivateKey::toPublic(PubKey& pubkey) const
  {
    return crypto_scalarmult_ed25519_base_noclamp(pubkey.data(), data()) != -1;
  }

  PrivateKey::~PrivateKey()
  {
    ::sodium_memzero(data(), size());
  }

  template <>
  bool
  SecretKey::SaveToFile(const fs::path& fname) const
  {
    std::string tmp(128, 0);
    llarp_buffer_t buf(tmp);
    if (!BEncode(&buf))
      return false;

    tmp.resize(buf.cur - buf.base);
    try
    {
      util::dump_file(fname, tmp);
    }
    catch (const std::exception&)
    {
      return false;
    }
    return true;
  }

  template <>
  bool
  IdentitySecret::LoadFromFile(const fs::path& fname)
  {
    std::array<byte_t, SIZE> buf;
    size_t sz;
    try
    {
      sz = util::slurp_file(fname, buf.data(), buf.size());
    }
    catch (const std::exception& e)
    {
      log::error(logcat, "failed to load service node seed: {}", e.what());
      return false;
    }
    if (sz != SIZE)
    {
      log::error(logcat, "service node seed size invalid: {} != {}", sz, SIZE);
      return false;
    }
    std::copy(buf.begin(), buf.end(), begin());
    return true;
  }

  IdentitySecret::~IdentitySecret()
  {
    ::sodium_memzero(data(), size());
  }

  byte_t*
  Signature::Lo()
  {
    return data();
  }

  const byte_t*
  Signature::Lo() const
  {
    return data();
  }

  byte_t*
  Signature::Hi()
  {
    return data() + 32;
  }

  const byte_t*
  Signature::Hi() const
  {
    return data() + 32;
  }

  PQKeyPair::~PQKeyPair()
  {
    ::sodium_memzero(data(), size());
  }
}  // namespace llarp
