#include "cyber/game/game_non_repudiation.hpp"

#include "cyber/game/app_payload_codec.hpp"

#include <stdexcept>
#include <string>

namespace cyber::game
{
AppCode app_code_for_game_message_type(GameMsgType type)
{
    switch (type)
    {
    case GameMsgType::join:
        return AppCode::game_join_req;
    case GameMsgType::move:
        return AppCode::game_move;
    case GameMsgType::target:
        return AppCode::game_target;
    case GameMsgType::shoot:
        return AppCode::game_shoot;
    case GameMsgType::state:
        return AppCode::game_state;
    default:
        throw std::runtime_error("unsupported game message type for non-repudiation");
    }
}

Packet build_signed_game_packet(EntityId src, EntityId dst, GameMsgType type,
                                const Bytes& payload, std::uint64_t kc_v,
                                bool encrypted, const RsaPrivateKey& private_key)
{
    const Bytes game_message = build_game_message({type, payload});
    const Bytes signed_payload =
        build_signed_app_payload(app_code_for_game_message_type(type), game_message, private_key);
    return make_packet(MsgType::app, src, dst,
                       encode_app_payload(signed_payload, kc_v, encrypted));
}

SignedAppPayload decode_signed_app_packet(const Packet& packet, std::uint64_t kc_v,
                                          bool encrypted)
{
    if (packet.msg_type != MsgType::app)
    {
        throw std::runtime_error("non-app packet cannot carry signed app payload");
    }
    return parse_signed_app_payload(decode_app_payload(packet.payload, kc_v, encrypted));
}

GameMessage parse_verified_game_message(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key)
{
    if (signed_payload.app_code == AppCode::app_ack)
    {
        throw std::runtime_error("APP_ACK is not a game message");
    }
    if (!verify_signed_app_payload(signed_payload, public_key))
    {
        throw std::runtime_error("signed game payload verification failed");
    }

    const GameMessage message = parse_game_message(signed_payload.app_payload);
    if (app_code_for_game_message_type(message.type) != signed_payload.app_code)
    {
        throw std::runtime_error("signed app code does not match game message type");
    }
    return message;
}

Packet build_signed_ack_packet(const Packet& received_packet,
                               const SignedAppPayload& received_signed_payload,
                               EntityId ack_src, EntityId ack_dst, std::uint64_t kc_v,
                               bool encrypted, const RsaPrivateKey& private_key)
{
    if (received_signed_payload.app_code == AppCode::app_ack)
    {
        throw std::runtime_error("APP_ACK packets are not acknowledged");
    }

    const AppAckPayload ack{received_packet.msg_type,
                            received_signed_payload.app_code,
                            received_packet.src,
                            received_packet.dst,
                            static_cast<std::uint32_t>(received_packet.payload.size()),
                            hash64(received_packet.payload)};
    const Bytes signed_ack =
        build_signed_app_payload(AppCode::app_ack, build_app_ack_payload(ack), private_key);
    return make_packet(MsgType::app, ack_src, ack_dst,
                       encode_app_payload(signed_ack, kc_v, encrypted));
}

AppAckPayload parse_verified_ack_payload(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key)
{
    if (signed_payload.app_code != AppCode::app_ack)
    {
        throw std::runtime_error("expected APP_ACK signed payload");
    }
    if (!verify_signed_app_payload(signed_payload, public_key))
    {
        throw std::runtime_error("APP_ACK signature verification failed");
    }
    return parse_app_ack_payload(signed_payload.app_payload);
}

void log_verified_ack_packet(Logger& ack_logger, std::string_view entity,
                             std::string_view thread_name, const Packet& packet)
{
    ack_logger.write(entity, thread_name, "APP_NON_REPUDIATION_ACK",
                     std::string("packet_hex=") + bytes_to_hex(serialize_packet(packet)));
}
} // namespace cyber::game
