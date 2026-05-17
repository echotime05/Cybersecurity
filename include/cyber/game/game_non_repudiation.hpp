#pragma once

#include "cyber/common/crypto.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/protocol_payloads.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstdint>
#include <string_view>

namespace cyber::game
{
// Game non-repudiation uses one envelope shape: AppCode + GameMessage is
// signed, then that signed payload is optionally encrypted with Kc_v. The
// fixed Packet header stays unchanged for routing and monitor visualization.
AppCode app_code_for_game_message_type(GameMsgType type);

Packet build_signed_game_packet(EntityId src, EntityId dst, GameMsgType type,
                                const Bytes& payload, std::uint64_t kc_v,
                                bool encrypted, const RsaPrivateKey& private_key);

SignedAppPayload decode_signed_app_packet(const Packet& packet, std::uint64_t kc_v,
                                          bool encrypted);

GameMessage parse_verified_game_message(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key);

// ACK evidence references the received wire payload by length and hash.
// APP_ACK packets are evidence records and are not acknowledged again.
Packet build_signed_ack_packet(const Packet& received_packet,
                               const SignedAppPayload& received_signed_payload,
                               EntityId ack_src, EntityId ack_dst, std::uint64_t kc_v,
                               bool encrypted, const RsaPrivateKey& private_key);

AppAckPayload parse_verified_ack_payload(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key);

void log_verified_ack_packet(Logger& ack_logger, std::string_view entity,
                             std::string_view thread_name, const Packet& packet);
} // namespace cyber::game
