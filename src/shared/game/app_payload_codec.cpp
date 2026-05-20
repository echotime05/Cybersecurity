#include "cyber/game/app_payload_codec.hpp"

#include "cyber/shared/crypto.hpp"

namespace cyber::game
{
// 最终游戏链路只加密 Packet.payload；固定头 MsgType/src/dst/payload_len/reserved 保持明文。
Bytes app_encode_payload(const Bytes& plain, std::uint64_t kc_v)
{
    return des_encrypt_payload(plain, kc_v);
}

// 解密最终游戏链路上的 Packet.payload。
Bytes app_decode_payload(const Bytes& wire, std::uint64_t kc_v)
{
    return des_decrypt_payload(wire, kc_v);
}
} // namespace cyber::game
