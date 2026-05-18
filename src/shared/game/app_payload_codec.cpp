#include "cyber/game/app_payload_codec.hpp"

#include "cyber/common/crypto.hpp"

namespace cyber::game
{
// The game encryption switch wraps only Packet.payload. MsgType, src, dst,
// payload_len, and reserved remain in the fixed network header.
Bytes app_encode_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return plain;
    }
    return des_encrypt_payload(plain, kc_v);
}

Bytes app_decode_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return wire;
    }
    return des_decrypt_payload(wire, kc_v);
}
} // namespace cyber::game
