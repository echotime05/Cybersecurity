#include "cyber/protocol/app_envelope.hpp"

#include "cyber/protocol/binary_codec.hpp"

namespace cyber
{
namespace
{
using protocol::detail::binary_read_bytes_u16;
using protocol::detail::binary_read_u32;
using protocol::detail::binary_read_u64;
using protocol::detail::binary_require_end;
using protocol::detail::binary_write_bytes_u16;
using protocol::detail::binary_write_u32;
using protocol::detail::binary_write_u64;
} // namespace

Bytes ack_build_payload(const AppAckPayload& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.acked_msg_type));
    out.push_back(static_cast<std::uint8_t>(value.acked_app_code));
    out.push_back(static_cast<std::uint8_t>(value.acked_src));
    out.push_back(static_cast<std::uint8_t>(value.acked_dst));
    binary_write_u32(out, value.acked_payload_len);
    binary_write_u64(out, value.acked_payload_hash);
    return out;
}

AppAckPayload ack_parse_payload(const Bytes& payload)
{
    std::size_t offset = 0;
    AppAckPayload value;
    value.acked_msg_type = static_cast<MsgType>(payload.at(offset++));
    value.acked_app_code = static_cast<AppCode>(payload.at(offset++));
    value.acked_src = static_cast<EntityId>(payload.at(offset++));
    value.acked_dst = static_cast<EntityId>(payload.at(offset++));
    value.acked_payload_len = binary_read_u32(payload, offset, "payload");
    value.acked_payload_hash = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "APP_ACK");
    return value;
}

// Signature input is the logical application bytes only: AppCode followed by
// app_payload. Packet header fields are not part of the RSA signature.
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload)
{
    Bytes logical;
    logical.push_back(static_cast<std::uint8_t>(code));
    logical.insert(logical.end(), app_payload.begin(), app_payload.end());
    return logical;
}

Bytes app_build_signed_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key)
{
    const Bytes logical = app_build_signed_logical_bytes(code, app_payload);
    const Bytes signature = rsa_sign_hash(hash64(logical), private_key);
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(code));
    binary_write_bytes_u16(out, app_payload);
    binary_write_bytes_u16(out, signature);
    return out;
}

SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload)
{
    std::size_t offset = 0;
    SignedAppPayload value;
    value.app_code = static_cast<AppCode>(decrypted_payload.at(offset++));
    value.app_payload = binary_read_bytes_u16(decrypted_payload, offset, "payload");
    value.signature = binary_read_bytes_u16(decrypted_payload, offset, "payload");
    binary_require_end(decrypted_payload, offset, "SIGNED_APP");
    return value;
}

bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key)
{
    return rsa_verify_hash(hash64(app_build_signed_logical_bytes(signed_payload.app_code,
                                                           signed_payload.app_payload)),
                           signed_payload.signature, public_key);
}
} // namespace cyber
