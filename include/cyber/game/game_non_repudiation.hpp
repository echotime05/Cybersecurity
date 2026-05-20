#pragma once

#include "cyber/shared/crypto.hpp"
#include "cyber/protocol/app_envelope.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstdint>
#include <string_view>

namespace cyber::game
{
// 游戏不可否认统一使用一层封装：AppCode + GameMessage 先签名，再用 Kc_v 加密。
// 固定 Packet 头保持明文，用于路由和协议可视化。
// 将游戏层 GameMsgType 映射到 MSG_APP 外层 AppCode。
AppCode app_map_game_message_code(GameMsgType type);

// 构造已签名且已加密的游戏报文，Client 和 V 发送游戏消息都走这里。
Packet app_build_signed_game_packet(EntityId src, EntityId dst, GameMsgType type,
                                    const Bytes& payload, std::uint64_t kc_v,
                                    const RsaPrivateKey& private_key);

// 解密 MSG_APP payload，并解析出 AppCode/app_payload/signature。
SignedAppPayload app_decode_signed_packet(const Packet& packet, std::uint64_t kc_v);

// 验证签名后解析游戏层 GameMessage，失败会抛出协议异常。
GameMessage app_parse_verified_game_message(const SignedAppPayload& signed_payload,
                                            const RsaPublicKey& public_key);

// 构造 ACK 证据报文，ACK 引用收到的原始 wire payload 长度和 hash。
// APP_ACK 本身是证据记录，不再被二次 ACK。
Packet ack_build_signed_packet(const Packet& received_packet,
                               const SignedAppPayload& received_signed_payload,
                               EntityId ack_src, EntityId ack_dst, std::uint64_t kc_v,
                               const RsaPrivateKey& private_key);

// 验签并解析 ACK 证据 payload。
AppAckPayload ack_parse_verified_payload(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key);
} // namespace cyber::game
