#include "cyber/common/crypto.hpp"
#include "cyber/common/packet.hpp"
#include "cyber/common/types.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        const cyber::Bytes short_plain = {0x41, 0x42, 0x43};
        const cyber::Bytes exact_plain = {0, 1, 2, 3, 4, 5, 6, 7};
        const std::uint64_t key56 = 0x001c24deeecc136eULL;

        const cyber::Bytes short_cipher = cyber::des_encrypt_payload(short_plain, key56);
        require(short_cipher != short_plain, "short DES cipher should differ from plaintext");
        require(cyber::des_decrypt_payload(short_cipher, key56) == short_plain,
                "short DES roundtrip failed");

        const cyber::Bytes exact_cipher = cyber::des_encrypt_payload(exact_plain, key56);
        require(exact_cipher.size() == 16U, "8-byte plaintext should receive a full padding block");
        require(cyber::des_decrypt_payload(exact_cipher, key56) == exact_plain,
                "exact-block DES roundtrip failed");

        const std::uint64_t h1 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x03});
        const std::uint64_t h2 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x03});
        const std::uint64_t h3 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x04});
        require(h1 == h2, "hash64 must be stable");
        require(h1 != h3, "hash64 should distinguish nearby inputs");

        const cyber::RsaKeyPair pair = cyber::demo_rsa_key_pair_for(cyber::EntityId::client1);
        const cyber::Bytes signature = cyber::rsa_sign_hash(h1, pair.private_key);
        require(cyber::rsa_verify_hash(h1, signature, pair.public_key), "RSA verify failed");
        require(!cyber::rsa_verify_hash(h3, signature, pair.public_key),
                "RSA verify should reject a different hash");

        const cyber::RsaKeyPair ca_pair = cyber::ca_key_pair_from_hex(
            "0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3",
            "0x10001",
            "0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69");
        const cyber::Certificate cert =
            cyber::make_certificate(cyber::EntityId::client1, pair.public_key, ca_pair.private_key);
        require(cyber::verify_certificate(cert, ca_pair.public_key),
                "CA certificate verification failed");
        require(cert.subject_id == cyber::EntityId::client1, "certificate subject mismatch");

        std::cout << "crypto_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "crypto_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
