#include "cyber/protocol/certificate_messages.hpp"

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

Bytes cert_build_c2v_body(const CertC2VBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.client_id));
    write_bytes_u16(out, value.cert);
    return out;
}

CertC2VBody cert_parse_c2v_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertC2VBody value;
    value.client_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = read_bytes_u16(payload, offset);
    require_end(payload, offset, "CERT_C2V_BODY");
    return value;
}

Bytes cert_build_v2c_body(const CertV2CBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.v_id));
    write_bytes_u16(out, value.cert);
    return out;
}

CertV2CBody cert_parse_v2c_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertV2CBody value;
    value.v_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = read_bytes_u16(payload, offset);
    require_end(payload, offset, "CERT_V2C_BODY");
    return value;
}
} // namespace cyber
