#include "cyber/protocol/app_envelope.hpp"

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

Bytes ack_build_payload(const AppAckPayload& value)
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

AppAckPayload ack_parse_payload(const Bytes& payload)
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
    write_bytes_u16(out, app_payload);
    write_bytes_u16(out, signature);
    return out;
}

SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload)
{
    std::size_t offset = 0;
    SignedAppPayload value;
    value.app_code = static_cast<AppCode>(decrypted_payload.at(offset++));
    value.app_payload = read_bytes_u16(decrypted_payload, offset);
    value.signature = read_bytes_u16(decrypted_payload, offset);
    require_end(decrypted_payload, offset, "SIGNED_APP");
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
