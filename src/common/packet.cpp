#include "cyber/common/packet.hpp"

#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

namespace cyber
{
PacketError::PacketError(const std::string& message) : std::runtime_error(message)
{
}

namespace
{
std::uint8_t to_byte(MsgType type)
{
    return static_cast<std::uint8_t>(type);
}

std::uint8_t to_byte(EntityId id)
{
    return static_cast<std::uint8_t>(id);
}

void write_u32_be(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint32_t read_u32_be(const Bytes& bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes.at(offset)) << 24U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 8U) |
           static_cast<std::uint32_t>(bytes.at(offset + 3U));
}

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    return -1;
}
} // namespace

Bytes serialize_packet(const Packet& packet)
{
    if (packet.payload.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw PacketError("packet payload is too large");
    }

    Bytes out;
    out.reserve(kPacketHeaderSize + packet.payload.size());
    out.push_back(to_byte(packet.msg_type));
    out.push_back(to_byte(packet.src));
    out.push_back(to_byte(packet.dst));
    write_u32_be(out, static_cast<std::uint32_t>(packet.payload.size()));
    write_u32_be(out, packet.reserved);
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
    return out;
}

Packet parse_packet(const Bytes& bytes)
{
    if (bytes.size() < kPacketHeaderSize)
    {
        throw PacketError("packet is shorter than the fixed header");
    }

    Packet packet;
    packet.msg_type = static_cast<MsgType>(bytes[0]);
    packet.src = static_cast<EntityId>(bytes[1]);
    packet.dst = static_cast<EntityId>(bytes[2]);
    const std::uint32_t payload_len = read_u32_be(bytes, 3);
    packet.reserved = read_u32_be(bytes, 7);
    if (bytes.size() != kPacketHeaderSize + payload_len)
    {
        throw PacketError("packet payload length mismatch");
    }
    packet.payload.assign(bytes.begin() + kPacketHeaderSize, bytes.end());
    return packet;
}

Bytes make_error_payload(ErrorCode code, const std::string& message)
{
    Bytes payload;
    payload.reserve(1U + message.size());
    payload.push_back(static_cast<std::uint8_t>(code));
    payload.insert(payload.end(), message.begin(), message.end());
    return payload;
}

ErrorCode parse_error_code(const Bytes& payload)
{
    if (payload.empty())
    {
        throw PacketError("error payload is empty");
    }
    return static_cast<ErrorCode>(payload[0]);
}

Bytes make_app_payload(AppCode code, const Bytes& app_payload)
{
    Bytes payload;
    payload.reserve(1U + app_payload.size());
    payload.push_back(static_cast<std::uint8_t>(code));
    payload.insert(payload.end(), app_payload.begin(), app_payload.end());
    return payload;
}

AppCode parse_app_code(const Bytes& payload)
{
    if (payload.empty())
    {
        throw PacketError("app payload is empty");
    }
    return static_cast<AppCode>(payload[0]);
}

Bytes parse_app_payload(const Bytes& payload)
{
    if (payload.empty())
    {
        throw PacketError("app payload is empty");
    }
    return Bytes(payload.begin() + 1, payload.end());
}

std::string bytes_to_hex(const Bytes& bytes)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::uint8_t byte : bytes)
    {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Bytes bytes_from_hex(const std::string& hex)
{
    std::string clean;
    clean.reserve(hex.size());
    for (char ch : hex)
    {
        if (!std::isspace(static_cast<unsigned char>(ch)))
        {
            clean.push_back(ch);
        }
    }

    if (clean.rfind("0x", 0) == 0 || clean.rfind("0X", 0) == 0)
    {
        clean.erase(0, 2);
    }

    if (clean.size() % 2U != 0U)
    {
        clean.insert(clean.begin(), '0');
    }

    Bytes out;
    out.reserve(clean.size() / 2U);
    for (std::size_t i = 0; i < clean.size(); i += 2U)
    {
        int hi = hex_value(clean[i]);
        int lo = hex_value(clean[i + 1U]);
        if (hi < 0 || lo < 0)
        {
            throw PacketError("invalid hex string");
        }
        out.push_back(static_cast<std::uint8_t>((hi << 4U) | lo));
    }
    return out;
}
} // namespace cyber
