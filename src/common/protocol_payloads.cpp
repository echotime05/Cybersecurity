#include "cyber/common/protocol_payloads.hpp"

#include <limits>

namespace cyber
{
namespace
{
void write_u16(Bytes& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_u32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_u64(Bytes& out, std::uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

std::uint16_t read_u16(const Bytes& in, std::size_t& offset)
{
    if (offset + 2U > in.size())
    {
        throw PacketError("payload is too short for uint16");
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>((in[offset] << 8U) | in[offset + 1U]);
    offset += 2U;
    return value;
}

std::uint32_t read_u32(const Bytes& in, std::size_t& offset)
{
    if (offset + 4U > in.size())
    {
        throw PacketError("payload is too short for uint32");
    }
    const std::uint32_t value = (static_cast<std::uint32_t>(in[offset]) << 24U) |
                                (static_cast<std::uint32_t>(in[offset + 1U]) << 16U) |
                                (static_cast<std::uint32_t>(in[offset + 2U]) << 8U) |
                                static_cast<std::uint32_t>(in[offset + 3U]);
    offset += 4U;
    return value;
}

std::uint64_t read_u64(const Bytes& in, std::size_t& offset)
{
    if (offset + 8U > in.size())
    {
        throw PacketError("payload is too short for uint64");
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i)
    {
        value = (value << 8U) | in[offset + i];
    }
    offset += 8U;
    return value;
}

void write_bytes_u16(Bytes& out, const Bytes& bytes)
{
    if (bytes.size() > std::numeric_limits<std::uint16_t>::max())
    {
        throw PacketError("byte field is too large for uint16 length");
    }
    write_u16(out, static_cast<std::uint16_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

Bytes read_bytes_u16(const Bytes& in, std::size_t& offset)
{
    const std::uint16_t len = read_u16(in, offset);
    if (offset + len > in.size())
    {
        throw PacketError("length-prefixed byte field exceeds payload");
    }
    Bytes out(in.begin() + offset, in.begin() + offset + len);
    offset += len;
    return out;
}

void require_end(const Bytes& in, std::size_t offset, const char* name)
{
    if (offset != in.size())
    {
        throw PacketError(std::string(name) + " has trailing bytes");
    }
}
} // namespace

Bytes build_as_req(const AsReq& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idc));
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    write_u64(out, value.ts1);
    return out;
}

AsReq parse_as_req(const Bytes& payload)
{
    if (payload.size() != 10U)
    {
        throw PacketError("AS_REQ payload must be 10 bytes");
    }
    std::size_t offset = 0;
    AsReq value;
    value.idc = static_cast<EntityId>(payload[offset++]);
    value.idtgs = static_cast<EntityId>(payload[offset++]);
    value.ts1 = read_u64(payload, offset);
    require_end(payload, offset, "AS_REQ");
    return value;
}

Bytes build_ticket_tgs_body(const TicketTgsBody& value)
{
    Bytes out;
    write_u64(out, value.kc_tgs);
    out.push_back(static_cast<std::uint8_t>(value.idc));
    write_u32(out, value.adc);
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    write_u64(out, value.ts2);
    write_u64(out, value.lifetime2);
    return out;
}

TicketTgsBody parse_ticket_tgs_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TicketTgsBody value;
    value.kc_tgs = read_u64(payload, offset);
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = read_u32(payload, offset);
    value.idtgs = static_cast<EntityId>(payload.at(offset++));
    value.ts2 = read_u64(payload, offset);
    value.lifetime2 = read_u64(payload, offset);
    require_end(payload, offset, "Ticket_tgs");
    return value;
}

Bytes encrypt_ticket_tgs(const TicketTgsBody& value, std::uint64_t ktgs)
{
    return des_encrypt_payload(build_ticket_tgs_body(value), ktgs);
}

TicketTgsBody decrypt_ticket_tgs(const Bytes& cipher, std::uint64_t ktgs)
{
    return parse_ticket_tgs_body(des_decrypt_payload(cipher, ktgs));
}

Bytes build_as_rep_body(const AsRepBody& value)
{
    Bytes out;
    write_u64(out, value.kc_tgs);
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    write_u64(out, value.ts2);
    write_u64(out, value.lifetime2);
    write_bytes_u16(out, value.ticket_tgs);
    return out;
}

AsRepBody parse_as_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    AsRepBody value;
    value.kc_tgs = read_u64(payload, offset);
    value.idtgs = static_cast<EntityId>(payload.at(offset++));
    value.ts2 = read_u64(payload, offset);
    value.lifetime2 = read_u64(payload, offset);
    value.ticket_tgs = read_bytes_u16(payload, offset);
    require_end(payload, offset, "AS_REP_BODY");
    return value;
}

Bytes build_authenticator_body(const AuthenticatorBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idc));
    write_u32(out, value.adc);
    write_u64(out, value.ts);
    return out;
}

AuthenticatorBody parse_authenticator_body(const Bytes& payload)
{
    std::size_t offset = 0;
    AuthenticatorBody value;
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = read_u32(payload, offset);
    value.ts = read_u64(payload, offset);
    require_end(payload, offset, "Authenticator");
    return value;
}

Bytes encrypt_authenticator(const AuthenticatorBody& value, std::uint64_t key56)
{
    return des_encrypt_payload(build_authenticator_body(value), key56);
}

AuthenticatorBody decrypt_authenticator(const Bytes& cipher, std::uint64_t key56)
{
    return parse_authenticator_body(des_decrypt_payload(cipher, key56));
}

Bytes build_tgs_req(const TgsReq& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idv));
    write_bytes_u16(out, value.ticket_tgs);
    write_bytes_u16(out, value.authenticator_tgs);
    return out;
}

TgsReq parse_tgs_req(const Bytes& payload)
{
    std::size_t offset = 0;
    TgsReq value;
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ticket_tgs = read_bytes_u16(payload, offset);
    value.authenticator_tgs = read_bytes_u16(payload, offset);
    require_end(payload, offset, "TGS_REQ");
    return value;
}

Bytes build_ticket_v_body(const TicketVBody& value)
{
    Bytes out;
    write_u64(out, value.kc_v);
    out.push_back(static_cast<std::uint8_t>(value.idc));
    write_u32(out, value.adc);
    out.push_back(static_cast<std::uint8_t>(value.idv));
    write_u64(out, value.ts4);
    write_u64(out, value.lifetime4);
    return out;
}

TicketVBody parse_ticket_v_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TicketVBody value;
    value.kc_v = read_u64(payload, offset);
    value.idc = static_cast<EntityId>(payload.at(offset++));
    value.adc = read_u32(payload, offset);
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ts4 = read_u64(payload, offset);
    value.lifetime4 = read_u64(payload, offset);
    require_end(payload, offset, "Ticket_v");
    return value;
}

Bytes encrypt_ticket_v(const TicketVBody& value, std::uint64_t kv)
{
    return des_encrypt_payload(build_ticket_v_body(value), kv);
}

TicketVBody decrypt_ticket_v(const Bytes& cipher, std::uint64_t kv)
{
    return parse_ticket_v_body(des_decrypt_payload(cipher, kv));
}

Bytes build_tgs_rep_body(const TgsRepBody& value)
{
    Bytes out;
    write_u64(out, value.kc_v);
    out.push_back(static_cast<std::uint8_t>(value.idv));
    write_u64(out, value.ts4);
    write_bytes_u16(out, value.ticket_v);
    return out;
}

TgsRepBody parse_tgs_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    TgsRepBody value;
    value.kc_v = read_u64(payload, offset);
    value.idv = static_cast<EntityId>(payload.at(offset++));
    value.ts4 = read_u64(payload, offset);
    value.ticket_v = read_bytes_u16(payload, offset);
    require_end(payload, offset, "TGS_REP_BODY");
    return value;
}

Bytes build_v_auth_req(const VAuthReq& value)
{
    Bytes out;
    write_bytes_u16(out, value.ticket_v);
    write_bytes_u16(out, value.authenticator_v);
    return out;
}

VAuthReq parse_v_auth_req(const Bytes& payload)
{
    std::size_t offset = 0;
    VAuthReq value;
    value.ticket_v = read_bytes_u16(payload, offset);
    value.authenticator_v = read_bytes_u16(payload, offset);
    require_end(payload, offset, "V_AUTH_REQ");
    return value;
}

Bytes build_v_auth_rep_body(const VAuthRepBody& value)
{
    Bytes out;
    write_u64(out, value.ts5_plus_1);
    return out;
}

VAuthRepBody parse_v_auth_rep_body(const Bytes& payload)
{
    std::size_t offset = 0;
    VAuthRepBody value;
    value.ts5_plus_1 = read_u64(payload, offset);
    require_end(payload, offset, "V_AUTH_REP_BODY");
    return value;
}

Bytes build_cert_c2v_body(const CertC2VBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.client_id));
    write_bytes_u16(out, value.cert);
    return out;
}

CertC2VBody parse_cert_c2v_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertC2VBody value;
    value.client_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = read_bytes_u16(payload, offset);
    require_end(payload, offset, "CERT_C2V_BODY");
    return value;
}

Bytes build_cert_v2c_body(const CertV2CBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.v_id));
    write_bytes_u16(out, value.cert);
    return out;
}

CertV2CBody parse_cert_v2c_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertV2CBody value;
    value.v_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = read_bytes_u16(payload, offset);
    require_end(payload, offset, "CERT_V2C_BODY");
    return value;
}

Bytes build_app_ack_payload(const AppAckPayload& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.acked_msg_type));
    out.push_back(static_cast<std::uint8_t>(value.acked_app_code));
    out.push_back(static_cast<std::uint8_t>(value.acked_src));
    out.push_back(static_cast<std::uint8_t>(value.acked_dst));
    write_u32(out, value.acked_payload_len);
    write_u64(out, value.acked_payload_hash);
    return out;
}

AppAckPayload parse_app_ack_payload(const Bytes& payload)
{
    std::size_t offset = 0;
    AppAckPayload value;
    value.acked_msg_type = static_cast<MsgType>(payload.at(offset++));
    value.acked_app_code = static_cast<AppCode>(payload.at(offset++));
    value.acked_src = static_cast<EntityId>(payload.at(offset++));
    value.acked_dst = static_cast<EntityId>(payload.at(offset++));
    value.acked_payload_len = read_u32(payload, offset);
    value.acked_payload_hash = read_u64(payload, offset);
    require_end(payload, offset, "APP_ACK");
    return value;
}

Bytes signed_app_logical_bytes(AppCode code, const Bytes& app_payload)
{
    Bytes logical;
    logical.push_back(static_cast<std::uint8_t>(code));
    logical.insert(logical.end(), app_payload.begin(), app_payload.end());
    return logical;
}

Bytes build_signed_app_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key)
{
    const Bytes logical = signed_app_logical_bytes(code, app_payload);
    const Bytes signature = rsa_sign_hash(hash64(logical), private_key);
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(code));
    write_bytes_u16(out, app_payload);
    write_bytes_u16(out, signature);
    return out;
}

SignedAppPayload parse_signed_app_payload(const Bytes& decrypted_payload)
{
    std::size_t offset = 0;
    SignedAppPayload value;
    value.app_code = static_cast<AppCode>(decrypted_payload.at(offset++));
    value.app_payload = read_bytes_u16(decrypted_payload, offset);
    value.signature = read_bytes_u16(decrypted_payload, offset);
    require_end(decrypted_payload, offset, "SIGNED_APP");
    return value;
}

bool verify_signed_app_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key)
{
    return rsa_verify_hash(hash64(signed_app_logical_bytes(signed_payload.app_code,
                                                           signed_payload.app_payload)),
                           signed_payload.signature, public_key);
}
} // namespace cyber
