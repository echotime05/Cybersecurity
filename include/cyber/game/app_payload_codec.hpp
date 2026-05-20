#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>

namespace cyber::game
{
// 用 Kc_v 加密应用层明文 payload，当前游戏链路默认启用。
Bytes app_encode_payload(const Bytes& plain, std::uint64_t kc_v);
// 用 Kc_v 解密网络上的应用层密文 payload。
Bytes app_decode_payload(const Bytes& wire, std::uint64_t kc_v);
} // namespace cyber::game
