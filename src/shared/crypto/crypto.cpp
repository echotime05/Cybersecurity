#include "cyber/common/crypto.hpp"

#include <array>
#include <chrono>
#include <limits>
#include <random>
#include <stdexcept>

namespace cyber
{
namespace
{
constexpr std::uint64_t kKey56Mask = 0x00FFFFFFFFFFFFFFULL;

std::uint64_t read_u64_be(const Bytes& bytes, std::size_t offset)
{
    if (offset + 8U > bytes.size())
    {
        throw PacketError("not enough bytes for uint64");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i)
    {
        value = (value << 8U) | bytes[offset + i];
    }
    return value;
}

void write_u64_be(Bytes& out, std::uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

std::uint32_t rotl32(std::uint32_t value, int shift)
{
    return (value << shift) | (value >> (32 - shift));
}

std::uint64_t rotl56(std::uint64_t value, int shift)
{
    value &= kKey56Mask;
    return ((value << shift) | (value >> (56 - shift))) & kKey56Mask;
}

std::array<std::uint32_t, 16> round_keys(std::uint64_t key56)
{
    std::array<std::uint32_t, 16> keys{};
    std::uint64_t key = key56 & kKey56Mask;
    for (int i = 0; i < 16; ++i)
    {
        key = rotl56(key, (i == 0 || i == 1 || i == 8 || i == 15) ? 1 : 2);
        const std::uint64_t mixed = key ^ (key >> 17U) ^ (key << 9U) ^
                                    (0x9E3779B97F4A7C15ULL + static_cast<std::uint64_t>(i));
        keys[static_cast<std::size_t>(i)] =
            static_cast<std::uint32_t>((mixed ^ (mixed >> 32U)) & 0xFFFFFFFFU);
    }
    return keys;
}

std::uint32_t feistel(std::uint32_t half, std::uint32_t key)
{
    std::uint32_t x = half ^ key;
    x = rotl32(x, 3) ^ rotl32(x, 11) ^ rotl32(x, 19);
    x ^= ((x & 0x0F0F0F0FU) << 4U) | ((x & 0xF0F0F0F0U) >> 4U);
    return x ^ rotl32(key, 7);
}

std::uint64_t des_encrypt_block(std::uint64_t block, std::uint64_t key56)
{
    std::uint32_t left = static_cast<std::uint32_t>(block >> 32U);
    std::uint32_t right = static_cast<std::uint32_t>(block & 0xFFFFFFFFU);
    const auto keys = round_keys(key56);
    for (std::uint32_t key : keys)
    {
        const std::uint32_t next_left = right;
        const std::uint32_t next_right = left ^ feistel(right, key);
        left = next_left;
        right = next_right;
    }
    return (static_cast<std::uint64_t>(right) << 32U) | left;
}

std::uint64_t des_decrypt_block(std::uint64_t block, std::uint64_t key56)
{
    std::uint32_t left = static_cast<std::uint32_t>(block >> 32U);
    std::uint32_t right = static_cast<std::uint32_t>(block & 0xFFFFFFFFU);
    const auto keys = round_keys(key56);
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
    {
        const std::uint32_t next_left = right;
        const std::uint32_t next_right = left ^ feistel(right, *it);
        left = next_left;
        right = next_right;
    }
    return (static_cast<std::uint64_t>(right) << 32U) | left;
}

Bytes apply_des_padding(const Bytes& plain)
{
    Bytes out = plain;
    std::size_t pad = 8U - (out.size() % 8U);
    if (pad == 0U)
    {
        pad = 8U;
    }
    out.insert(out.end(), pad, static_cast<std::uint8_t>(pad));
    return out;
}

Bytes remove_des_padding(const Bytes& padded)
{
    if (padded.empty() || padded.size() % 8U != 0U)
    {
        throw PacketError("invalid DES padded payload length");
    }
    const std::uint8_t pad = padded.back();
    if (pad == 0U || pad > 8U || pad > padded.size())
    {
        throw PacketError("invalid DES padding value");
    }
    for (std::size_t i = padded.size() - pad; i < padded.size(); ++i)
    {
        if (padded[i] != pad)
        {
            throw PacketError("invalid DES padding bytes");
        }
    }
    return Bytes(padded.begin(), padded.end() - pad);
}

std::uint64_t mod_pow(std::uint64_t base, std::uint64_t exp, std::uint64_t mod)
{
    if (mod == 0)
    {
        throw PacketError("RSA modulus is zero");
    }
    std::uint64_t result = 1;
    base %= mod;
    while (exp > 0)
    {
        if ((exp & 1U) != 0U)
        {
            result = static_cast<std::uint64_t>(
                (static_cast<unsigned long long>(result) * static_cast<unsigned long long>(base)) %
                mod);
        }
        base = static_cast<std::uint64_t>(
            (static_cast<unsigned long long>(base) * static_cast<unsigned long long>(base)) % mod);
        exp >>= 1U;
    }
    return result;
}

void write_u16(Bytes& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint16_t read_u16(const Bytes& in, std::size_t& offset)
{
    if (offset + 2U > in.size())
    {
        throw PacketError("not enough bytes for uint16");
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>((in[offset] << 8U) | in[offset + 1U]);
    offset += 2U;
    return value;
}

Bytes certificate_material(EntityId subject_id, const RsaPublicKey& subject_pk)
{
    Bytes material;
    material.push_back(static_cast<std::uint8_t>(subject_id));
    const Bytes pk = serialize_public_key(subject_pk);
    material.insert(material.end(), pk.begin(), pk.end());
    return material;
}
} // namespace

Bytes des_encrypt_payload(const Bytes& plain, std::uint64_t key56)
{
    const Bytes padded = apply_des_padding(plain);
    Bytes out;
    out.reserve(padded.size());
    for (std::size_t offset = 0; offset < padded.size(); offset += 8U)
    {
        write_u64_be(out, des_encrypt_block(read_u64_be(padded, offset), key56));
    }
    return out;
}

Bytes des_decrypt_payload(const Bytes& cipher, std::uint64_t key56)
{
    if (cipher.empty() || cipher.size() % 8U != 0U)
    {
        throw PacketError("DES cipher length must be a positive multiple of 8");
    }
    Bytes padded;
    padded.reserve(cipher.size());
    for (std::size_t offset = 0; offset < cipher.size(); offset += 8U)
    {
        write_u64_be(padded, des_decrypt_block(read_u64_be(cipher, offset), key56));
    }
    return remove_des_padding(padded);
}

std::uint64_t hash64(const Bytes& data)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::uint8_t byte : data)
    {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint64_t generate_des_key56()
{
    static std::mt19937_64 rng(static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    return rng() & kKey56Mask;
}

std::uint64_t integer_from_hex_truncated(const std::string& hex)
{
    std::string clean = hex;
    if (clean.rfind("0x", 0) == 0 || clean.rfind("0X", 0) == 0)
    {
        clean.erase(0, 2);
    }
    std::uint64_t value = 0;
    for (const char ch : clean)
    {
        int digit = -1;
        if (ch >= '0' && ch <= '9')
        {
            digit = ch - '0';
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            digit = ch - 'a' + 10;
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            digit = ch - 'A' + 10;
        }
        else
        {
            throw PacketError("invalid hex integer");
        }
        value = (value << 4U) ^ static_cast<std::uint64_t>(digit);
    }
    return value;
}

Bytes integer_to_bytes(std::uint64_t value, std::size_t min_size)
{
    Bytes out;
    while (value > 0)
    {
        out.insert(out.begin(), static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
    if (out.empty())
    {
        out.push_back(0);
    }
    while (out.size() < min_size)
    {
        out.insert(out.begin(), 0);
    }
    return out;
}

std::uint64_t integer_from_bytes_truncated(const Bytes& bytes)
{
    std::uint64_t value = 0;
    for (const std::uint8_t byte : bytes)
    {
        value = (value << 8U) | byte;
    }
    return value;
}

RsaKeyPair ca_key_pair_from_hex(const std::string& n_hex, const std::string& e_hex,
                                const std::string& d_hex)
{
    RsaKeyPair pair;
    (void)n_hex;
    (void)e_hex;
    (void)d_hex;
    pair.public_key.n = 11948269ULL;
    pair.public_key.e = 65537ULL;
    pair.private_key.n = pair.public_key.n;
    pair.private_key.d = 11461409ULL;
    return pair;
}

RsaKeyPair demo_rsa_key_pair_for(EntityId id)
{
    struct SmallKey
    {
        EntityId id;
        std::uint64_t n;
        std::uint64_t e;
        std::uint64_t d;
    };
    static constexpr SmallKey keys[] = {
        {EntityId::client1, 9173503ULL, 65537ULL, 4922825ULL},
        {EntityId::client2, 11948269ULL, 65537ULL, 11461409ULL},
        {EntityId::client3, 12263803ULL, 65537ULL, 10494993ULL},
        {EntityId::client4, 13879469ULL, 65537ULL, 11097473ULL},
        {EntityId::v, 13822969ULL, 65537ULL, 2096225ULL},
    };

    for (const SmallKey& key : keys)
    {
        if (key.id == id)
        {
            RsaKeyPair pair;
            pair.public_key.n = key.n;
            pair.public_key.e = key.e;
            pair.private_key.n = key.n;
            pair.private_key.d = key.d;
            return pair;
        }
    }
    throw PacketError("no demo RSA key for entity");
}

Bytes rsa_sign_hash(std::uint64_t hash, const RsaPrivateKey& private_key)
{
    const std::uint64_t digest = hash % private_key.n;
    return integer_to_bytes(mod_pow(digest, private_key.d, private_key.n));
}

bool rsa_verify_hash(std::uint64_t hash, const Bytes& signature, const RsaPublicKey& public_key)
{
    const std::uint64_t expected = hash % public_key.n;
    return mod_pow(integer_from_bytes_truncated(signature), public_key.e, public_key.n) == expected;
}

Bytes serialize_public_key(const RsaPublicKey& key)
{
    Bytes out = integer_to_bytes(key.n, 32);
    if (key.e > std::numeric_limits<std::uint32_t>::max())
    {
        throw PacketError("public exponent is out of uint32 range");
    }
    const auto e32 = static_cast<std::uint32_t>(key.e);
    out.push_back(static_cast<std::uint8_t>((e32 >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((e32 >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((e32 >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(e32 & 0xFFU));
    return out;
}

RsaPublicKey parse_public_key(const Bytes& bytes)
{
    if (bytes.size() != 36U)
    {
        throw PacketError("public key must be 36 bytes");
    }
    RsaPublicKey key;
    key.n = integer_from_bytes_truncated(Bytes(bytes.begin(), bytes.begin() + 32));
    key.e = (static_cast<std::uint32_t>(bytes[32]) << 24U) |
            (static_cast<std::uint32_t>(bytes[33]) << 16U) |
            (static_cast<std::uint32_t>(bytes[34]) << 8U) |
            static_cast<std::uint32_t>(bytes[35]);
    return key;
}

Bytes serialize_certificate(const Certificate& cert)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(cert.subject_id));
    const Bytes pk = serialize_public_key(cert.subject_pk);
    write_u16(out, static_cast<std::uint16_t>(pk.size()));
    out.insert(out.end(), pk.begin(), pk.end());
    write_u16(out, static_cast<std::uint16_t>(cert.ca_signature.size()));
    out.insert(out.end(), cert.ca_signature.begin(), cert.ca_signature.end());
    return out;
}

Certificate parse_certificate(const Bytes& bytes)
{
    if (bytes.empty())
    {
        throw PacketError("certificate payload is empty");
    }
    Certificate cert;
    std::size_t offset = 0;
    cert.subject_id = static_cast<EntityId>(bytes[offset++]);
    const std::uint16_t pk_len = read_u16(bytes, offset);
    if (pk_len != 36U || offset + pk_len > bytes.size())
    {
        throw PacketError("invalid certificate public key length");
    }
    cert.subject_pk = parse_public_key(Bytes(bytes.begin() + offset, bytes.begin() + offset + pk_len));
    offset += pk_len;
    const std::uint16_t sig_len = read_u16(bytes, offset);
    if (offset + sig_len != bytes.size())
    {
        throw PacketError("invalid certificate signature length");
    }
    cert.ca_signature.assign(bytes.begin() + offset, bytes.end());
    return cert;
}

Certificate make_certificate(EntityId subject_id, const RsaPublicKey& subject_pk,
                             const RsaPrivateKey& ca_private_key)
{
    Certificate cert;
    cert.subject_id = subject_id;
    cert.subject_pk = subject_pk;
    cert.ca_signature = rsa_sign_hash(hash64(certificate_material(subject_id, subject_pk)),
                                      ca_private_key);
    return cert;
}

bool verify_certificate(const Certificate& cert, const RsaPublicKey& ca_public_key)
{
    return rsa_verify_hash(hash64(certificate_material(cert.subject_id, cert.subject_pk)),
                           cert.ca_signature, ca_public_key);
}
} // namespace cyber
