#include <llarp/crypto/crypto_libsodium.hpp>

#include <iostream>

#include <catch2/catch.hpp>

using namespace llarp;

TEST_CASE("Identity key")
{
  llarp::sodium::CryptoLibSodium crypto;
  SecretKey secret;
  crypto.identity_keygen(secret);

  SECTION("Keygen")
  {
    REQUIRE_FALSE(secret.IsZero());
  }

  SECTION("Sign-verify")
  {
    AlignedBuffer<32> random;
    Randomize(random);
    Signature sig;
    const PubKey pk = secret.toPublic();

    const llarp_buffer_t buf(random.data(), random.size());
    REQUIRE(crypto.sign(sig, secret, buf));
    REQUIRE(crypto.verify(pk, buf, sig));
    Randomize(random);
    // mangle body
    REQUIRE_FALSE(crypto.verify(pk, buf, sig));
  }
}

TEST_CASE("PQ crypto")
{
  llarp::sodium::CryptoLibSodium crypto;
  PQKeyPair keys;
  crypto.pqe_keygen(keys);
  PQCipherBlock block;
  SharedSecret shared, otherShared;
  auto c = &crypto;

  REQUIRE(keys.size() == PQ_KEYPAIRSIZE);
  PQPubKey pk{};
  std::copy_n(pq_keypair_to_public(keys), pk.size(), pk.begin());
  REQUIRE(c->pqe_encrypt(block, shared, pk));
  REQUIRE(c->pqe_decrypt(block, otherShared, pq_keypair_to_secret(keys)));
  REQUIRE(otherShared == shared);
}

#ifdef HAVE_CRYPT

TEST_CASE("passwd hash valid")
{
  llarp::sodium::CryptoLibSodium crypto;

  // poggers password hashes
  std::set<std::string> valid_hashes;
  // UNIX DES
  valid_hashes.emplace("CVu85Ms694POo");
  // sha256 salted
  valid_hashes.emplace(
      "$5$cIghotiBGjfPC7Fu$"
      "TXXxPhpUcEiF9tMnjhEVJFi9AlNDSkNRQFTrXPQTKS9");
  // sha512 salted
  valid_hashes.emplace(
      "$6$qB77ms3wCIo.xVKP$Hl0RLuDgWNmIW4s."
      "5KUbFmnauoTfrWSPJzDCD8ZTSSfwRbMgqgG6F9y3K.YEYVij8g/"
      "Js0DRT2RhgXoX0sHGb.");

  for (const auto& hash : valid_hashes)
  {
    // make sure it is poggers ...
    REQUIRE(crypto.check_passwd_hash(hash, "poggers"));
    // ... and not inscrutible
    REQUIRE(not crypto.check_passwd_hash(hash, "inscrutible"));
  }
}

TEST_CASE("passwd hash malformed")
{
  llarp::sodium::CryptoLibSodium crypto;

  std::set<std::string> invalid_hashes = {
      "stevejobs",
      "$JKEDbzgzym1N6",  // crypt() for "stevejobs" with a $ at the begining
      "$0$zero$AAAAAAAAAAA",
      "$$$AAAAAAAAAAAA",
      "$LIGMA$BALLS$LMAOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO."};
  for (const auto& hash : invalid_hashes)
    REQUIRE(not crypto.check_passwd_hash(hash, "stevejobs"));
}

#endif
