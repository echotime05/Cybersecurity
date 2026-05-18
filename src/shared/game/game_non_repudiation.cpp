#include "cyber/game/game_non_repudiation.hpp"

#include "cyber/game/app_payload_codec.hpp"

#include <stdexcept>
#include <string>

namespace cyber::game
{
AppCode app_map_game_message_code(GameMsgType type)
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

Packet app_build_signed_game_packet(EntityId src, EntityId dst, GameMsgType type,
                                const Bytes& payload, std::uint64_t kc_v,
                                bool encrypted, const RsaPrivateKey& private_key)
{
    const Bytes game_message = game_build_message({type, payload});
    const Bytes signed_payload =
        app_build_signed_payload(app_map_game_message_code(type), game_message, private_key);
    return make_packet(MsgType::app, src, dst,
                       app_encode_payload(signed_payload, kc_v, encrypted));
}

SignedAppPayload app_decode_signed_packet(const Packet& packet, std::uint64_t kc_v,
                                          bool encrypted)
{
    if (packet.msg_type != MsgType::app)
    {
        throw std::runtime_error("non-app packet cannot carry signed app payload");
    }
    return app_parse_signed_payload(app_decode_payload(packet.payload, kc_v, encrypted));
}

GameMessage app_parse_verified_game_message(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key)
{
    if (signed_payload.app_code == AppCode::app_ack)
    {
        throw std::runtime_error("APP_ACK is not a game message");
    }
    if (!app_verify_signed_payload(signed_payload, public_key))
    {
        throw std::runtime_error("signed game payload verification failed");
    }

    const GameMessage message = game_parse_message(signed_payload.app_payload);
    if (app_map_game_message_code(message.type) != signed_payload.app_code)
    {
        throw std::runtime_error("signed app code does not match game message type");
    }
    return message;
}

Packet ack_build_signed_packet(const Packet& received_packet,
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
        app_build_signed_payload(AppCode::app_ack, ack_build_payload(ack), private_key);
    return make_packet(MsgType::app, ack_src, ack_dst,
                       app_encode_payload(signed_ack, kc_v, encrypted));
}

AppAckPayload ack_parse_verified_payload(const SignedAppPayload& signed_payload,
                                         const RsaPublicKey& public_key)
{
    if (signed_payload.app_code != AppCode::app_ack)
    {
        throw std::runtime_error("expected APP_ACK signed payload");
    }
    if (!app_verify_signed_payload(signed_payload, public_key))
    {
        throw std::runtime_error("APP_ACK signature verification failed");
    }
    return ack_parse_payload(signed_payload.app_payload);
}
} // namespace cyber::game
