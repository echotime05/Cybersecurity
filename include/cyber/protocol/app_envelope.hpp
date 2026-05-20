#pragma once

#include "cyber/shared/crypto.hpp"
#include "cyber/protocol/packet.hpp"

#include <cstdint>

namespace cyber
{
// APP_ACK 的明文结构，用原报文类型、长度和 payload hash 指向被确认的报文。
struct AppAckPayload
{
    MsgType acked_msg_type = MsgType::app;
    AppCode acked_app_code = AppCode::app_ack;
    EntityId acked_src = EntityId::unknown;
    EntityId acked_dst = EntityId::unknown;
    std::uint32_t acked_payload_len = 0;
    std::uint64_t acked_payload_hash = 0;
};

// 应用层加密前的签名封装：AppCode + app_payload + signature。
struct SignedAppPayload
{
    AppCode app_code = AppCode::app_ack;
    Bytes app_payload;
    Bytes signature;
};

// 序列化 APP_ACK 明文 payload。
Bytes ack_build_payload(const AppAckPayload& value);
// 解析 APP_ACK 明文 payload。
AppAckPayload ack_parse_payload(const Bytes& payload);
// 对 AppCode 和 app_payload 的摘要签名，并构造签名封装。
Bytes app_build_signed_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key);
// 从解密后的应用层 payload 中解析签名封装。
SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload);
// 用发送方公钥验证签名封装是否可信。
bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key);
// 构造签名前的逻辑字节，保证签名和验签使用同一份内容。
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload);
} // namespace cyber
