#include "cyber/game/app_payload_codec.hpp"

#include "cyber/common/crypto.hpp"

namespace cyber::game
{
Bytes encode_app_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return plain;
    }
    return des_encrypt_payload(plain, kc_v);
}

Bytes decode_app_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return wire;
    }
    return des_decrypt_payload(wire, kc_v);
}
} // namespace cyber::game
