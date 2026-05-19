#pragma once

#include "cyber/protocol/packet.hpp"
#include "cyber/shared/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
struct RsaPublicKey
{
    std::uint64_t n = 0;
    std::uint64_t e = 0;
};

struct RsaPrivateKey
{
    std::uint64_t n = 0;
    std::uint64_t d = 0;
};

struct RsaKeyPair
{
    RsaPublicKey public_key;
    RsaPrivateKey private_key;
};

struct Certificate
{
    EntityId subject_id = EntityId::unknown;
    RsaPublicKey subject_pk;
    Bytes ca_signature;
};

Bytes des_encrypt_payload(const Bytes& plain, std::uint64_t key56);
Bytes des_decrypt_payload(const Bytes& cipher, std::uint64_t key56);

std::uint64_t hash64(const Bytes& data);
std::uint64_t generate_des_key56();

std::uint64_t integer_from_hex_truncated(const std::string& hex);
Bytes integer_to_bytes(std::uint64_t value, std::size_t min_size = 0);
std::uint64_t integer_from_bytes_truncated(const Bytes& bytes);

RsaKeyPair ca_key_pair_from_hex(const std::string& n_hex, const std::string& e_hex,
                                const std::string& d_hex);
RsaKeyPair demo_rsa_key_pair_for(EntityId id);
Bytes rsa_sign_hash(std::uint64_t hash, const RsaPrivateKey& private_key);
bool rsa_verify_hash(std::uint64_t hash, const Bytes& signature, const RsaPublicKey& public_key);

Bytes serialize_public_key(const RsaPublicKey& key);
RsaPublicKey parse_public_key(const Bytes& bytes);
Bytes serialize_certificate(const Certificate& cert);
Certificate parse_certificate(const Bytes& bytes);
Certificate make_certificate(EntityId subject_id, const RsaPublicKey& subject_pk,
                             const RsaPrivateKey& ca_private_key);
bool verify_certificate(const Certificate& cert, const RsaPublicKey& ca_public_key);
} // namespace cyber
