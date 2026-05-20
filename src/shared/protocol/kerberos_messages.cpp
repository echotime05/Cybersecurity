#include "cyber/protocol/kerberos_messages.hpp"

#include "cyber/shared/crypto.hpp"
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

// 序列化 AS_REQ。
Bytes as_build_req(const AsReq& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idc));
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    binary_write_u64(out, value.ts1);
    return out;
}

// 解析 AS_REQ。
AsReq as_parse_req(const Bytes& payload)
{
    if (payload.size() != 10U)
    {
        throw PacketError("AS_REQ payload must be 10 bytes");
    }
    std::size_t offset = 0;
    AsReq value;
    value.idc = static_cast<EntityId>(payload[offset++]);
    value.idtgs = static_cast<EntityId>(payload[offset++]);
    value.ts1 = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "AS_REQ");
    return value;
}

// 序列化 ticket_tgs 的明文主体。
Bytes tgs_ticket_build_body(const TicketTgsBody& value)
{
    Bytes out;
    binary_write_u64(out, value.kc_tgs);
    out.push_back(static_cast<std::uint8_t>(value.idc));
    binary_write_u32(out, value.adc);
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    binary_write_u64(out, value.ts2);
    binary_write_u64(out, value.lifetime2);
    return out;
}

// 解析 ticket_tgs 的明文主体。
TicketTgsBody tgs_ticket_parse_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TicketTgsBody value;
    value.kc_tgs = binary_read_u64(payload, offset, "payload");
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = binary_read_u32(payload, offset, "payload");
    value.idtgs = static_cast<EntityId>(payload.at(offset++));
    value.ts2 = binary_read_u64(payload, offset, "payload");
    value.lifetime2 = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "Ticket_tgs");
    return value;
}

// 用 KTGS 加密 ticket_tgs。
Bytes tgs_ticket_encrypt(const TicketTgsBody& value, std::uint64_t ktgs)
{
    return des_encrypt_payload(tgs_ticket_build_body(value), ktgs);
}

// 用 KTGS 解密 ticket_tgs 并解析主体。
TicketTgsBody tgs_ticket_decrypt(const Bytes& cipher, std::uint64_t ktgs)
{
    return tgs_ticket_parse_body(des_decrypt_payload(cipher, ktgs));
}

// 序列化 AS_REP 中 Client 可解的明文主体。
Bytes as_build_rep_body(const AsRepBody& value)
{
    Bytes out;
    binary_write_u64(out, value.kc_tgs);
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    binary_write_u64(out, value.ts2);
    binary_write_u64(out, value.lifetime2);
    binary_write_bytes_u16(out, value.ticket_tgs);
    return out;
}

// 解析 AS_REP 中 Client 解密后的主体。
AsRepBody as_parse_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    AsRepBody value;
    value.kc_tgs = binary_read_u64(payload, offset, "payload");
    value.idtgs = static_cast<EntityId>(payload.at(offset++));
    value.ts2 = binary_read_u64(payload, offset, "payload");
    value.lifetime2 = binary_read_u64(payload, offset, "payload");
    value.ticket_tgs = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "AS_REP_BODY");
    return value;
}

// 序列化认证器明文主体。
Bytes authenticator_build_body(const AuthenticatorBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idc));
    binary_write_u32(out, value.adc);
    binary_write_u64(out, value.ts);
    return out;
}

// 解析认证器明文主体。
AuthenticatorBody authenticator_parse_body(const Bytes& payload)
{
    std::size_t offset = 0;
    AuthenticatorBody value;
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = binary_read_u32(payload, offset, "payload");
    value.ts = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "Authenticator");
    return value;
}

// 用会话密钥加密认证器。
Bytes authenticator_encrypt(const AuthenticatorBody& value, std::uint64_t key56)
{
    return des_encrypt_payload(authenticator_build_body(value), key56);
}

// 用会话密钥解密认证器并解析主体。
AuthenticatorBody authenticator_decrypt(const Bytes& cipher, std::uint64_t key56)
{
    return authenticator_parse_body(des_decrypt_payload(cipher, key56));
}

// 序列化 TGS_REQ。
Bytes tgs_build_req(const TgsReq& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idv));
    binary_write_bytes_u16(out, value.ticket_tgs);
    binary_write_bytes_u16(out, value.authenticator_tgs);
    return out;
}

// 解析 TGS_REQ。
TgsReq tgs_parse_req(const Bytes& payload)
{
    std::size_t offset = 0;
    TgsReq value;
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ticket_tgs = binary_read_bytes_u16(payload, offset, "payload");
    value.authenticator_tgs = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "TGS_REQ");
    return value;
}

// 序列化 ticket_v 的明文主体。
Bytes v_ticket_build_body(const TicketVBody& value)
{
    Bytes out;
    binary_write_u64(out, value.kc_v);
    out.push_back(static_cast<std::uint8_t>(value.idc));
    binary_write_u32(out, value.adc);
    out.push_back(static_cast<std::uint8_t>(value.idv));
    binary_write_u64(out, value.ts4);
    binary_write_u64(out, value.lifetime4);
    return out;
}

// 解析 ticket_v 的明文主体。
TicketVBody v_ticket_parse_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TicketVBody value;
    value.kc_v = binary_read_u64(payload, offset, "payload");
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = binary_read_u32(payload, offset, "payload");
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ts4 = binary_read_u64(payload, offset, "payload");
    value.lifetime4 = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "Ticket_v");
    return value;
}

// 用 KV 加密 ticket_v。
Bytes v_ticket_encrypt(const TicketVBody& value, std::uint64_t kv)
{
    return des_encrypt_payload(v_ticket_build_body(value), kv);
}

// 用 KV 解密 ticket_v 并解析主体。
TicketVBody v_ticket_decrypt(const Bytes& cipher, std::uint64_t kv)
{
    return v_ticket_parse_body(des_decrypt_payload(cipher, kv));
}

// 序列化 TGS_REP 中 Client 可解的明文主体。
Bytes tgs_build_rep_body(const TgsRepBody& value)
{
    Bytes out;
    binary_write_u64(out, value.kc_v);
    out.push_back(static_cast<std::uint8_t>(value.idv));
    binary_write_u64(out, value.ts4);
    binary_write_bytes_u16(out, value.ticket_v);
    return out;
}

// 解析 TGS_REP 中 Client 解密后的主体。
TgsRepBody tgs_parse_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TgsRepBody value;
    value.kc_v = binary_read_u64(payload, offset, "payload");
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ts4 = binary_read_u64(payload, offset, "payload");
    value.ticket_v = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "TGS_REP_BODY");
    return value;
}

// 序列化 V_AUTH_REQ。
Bytes v_auth_build_req(const VAuthReq& value)
{
    Bytes out;
    binary_write_bytes_u16(out, value.ticket_v);
    binary_write_bytes_u16(out, value.authenticator_v);
    return out;
}

// 解析 V_AUTH_REQ。
VAuthReq v_auth_parse_req(const Bytes& payload)
{
    std::size_t offset = 0;
    VAuthReq value;
    value.ticket_v = binary_read_bytes_u16(payload, offset, "payload");
    value.authenticator_v = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "V_AUTH_REQ");
    return value;
}

// 序列化 V_AUTH_REP 中 Client 可解的明文主体。
Bytes v_auth_build_rep_body(const VAuthRepBody& value)
{
    Bytes out;
    binary_write_u64(out, value.ts5_plus_1);
    return out;
}

// 解析 V_AUTH_REP 中 Client 解密后的主体。
VAuthRepBody v_auth_parse_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    VAuthRepBody value;
    value.ts5_plus_1 = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "V_AUTH_REP_BODY");
    return value;
}
} // namespace cyber
