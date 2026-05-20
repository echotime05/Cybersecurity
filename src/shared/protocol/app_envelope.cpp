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

// 序列化 APP_ACK 证据 payload。
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

// 解析 APP_ACK 证据 payload。
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

// 签名输入只包含逻辑应用层字节：AppCode 后接 app_payload；固定报文头不参与签名。
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload)
{
    Bytes logical;
    logical.push_back(static_cast<std::uint8_t>(code));
    logical.insert(logical.end(), app_payload.begin(), app_payload.end());
    return logical;
}

// 构造签名封装 payload：AppCode、app_payload 和 RSA 签名。
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

// 解析解密后的签名封装 payload。
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

// 验证签名封装中 app_code + app_payload 的 RSA 签名。
bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key)
{
    return rsa_verify_hash(hash64(app_build_signed_logical_bytes(signed_payload.app_code,
                                                           signed_payload.app_payload)),
                           signed_payload.signature, public_key);
}
} // namespace cyber
