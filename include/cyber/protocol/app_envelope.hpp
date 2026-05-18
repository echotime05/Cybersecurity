#pragma once

#include "cyber/common/crypto.hpp"
#include "cyber/protocol/packet.hpp"

#include <cstdint>

namespace cyber
{
struct AppAckPayload
{
    MsgType acked_msg_type = MsgType::app;
    AppCode acked_app_code = AppCode::app_ack;
    EntityId acked_src = EntityId::unknown;
    EntityId acked_dst = EntityId::unknown;
    std::uint32_t acked_payload_len = 0;
    std::uint64_t acked_payload_hash = 0;
};

struct SignedAppPayload
{
    AppCode app_code = AppCode::app_ack;
    Bytes app_payload;
    Bytes signature;
};

Bytes ack_build_payload(const AppAckPayload& value);
AppAckPayload ack_parse_payload(const Bytes& payload);
Bytes app_build_signed_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key);
SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload);
bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key);
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload);
} // namespace cyber
