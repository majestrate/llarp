#pragma once

#include <cstdint>
#include <oqs/kem.h>

static constexpr uint32_t PUBKEYSIZE = 32;
static constexpr uint32_t SECKEYSIZE = 64;
static constexpr uint32_t NONCESIZE = 24;
static constexpr uint32_t SHAREDKEYSIZE = 32;
static constexpr uint32_t HASHSIZE = 64;
static constexpr uint32_t SHORTHASHSIZE = 32;
static constexpr uint32_t HMACSECSIZE = 32;
static constexpr uint32_t SIGSIZE = 64;
static constexpr uint32_t TUNNONCESIZE = 32;
static constexpr uint32_t HMACSIZE = 32;
static constexpr uint32_t PATHIDSIZE = 16;

static constexpr uint32_t PQ_CIPHERTEXTSIZE = OQS_KEM_ntruprime_sntrup761_length_ciphertext;
static constexpr uint32_t PQ_PUBKEYSIZE = OQS_KEM_ntruprime_sntrup761_length_public_key;
static constexpr uint32_t PQ_SECRETKEYSIZE = OQS_KEM_ntruprime_sntrup761_length_secret_key;
static constexpr uint32_t PQ_KEYPAIRSIZE = (PQ_SECRETKEYSIZE + PQ_PUBKEYSIZE);
static constexpr uint32_t PQ_SHAREDKEYSIZE = OQS_KEM_ntruprime_sntrup761_length_shared_secret;

static_assert(PQ_SHAREDKEYSIZE == SHAREDKEYSIZE, "PQ_SHAREDKEYSIZE != SHAREDKEYSIZE");
