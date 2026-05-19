#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace cyber::protocol::detail
{
inline void binary_require_remaining(const Bytes& in, std::size_t offset,
                                     std::size_t count, std::string_view context)
{
    if (offset > in.size() || in.size() - offset < count)
    {
        throw PacketError(std::string(context) + " is too short");
    }
}

inline void binary_write_u16(Bytes& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

inline void binary_write_u32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

inline void binary_write_u64(Bytes& out, std::uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

inline void binary_write_f32(Bytes& out, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    binary_write_u32(out, bits);
}

inline std::uint16_t binary_read_u16(const Bytes& in, std::size_t& offset,
                                     std::string_view context)
{
    binary_require_remaining(in, offset, 2U, context);
    const std::uint16_t value =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[offset]) << 8U) |
                                   static_cast<std::uint16_t>(in[offset + 1U]));
    offset += 2U;
    return value;
}

inline std::uint32_t binary_read_u32(const Bytes& in, std::size_t& offset,
                                     std::string_view context)
{
    binary_require_remaining(in, offset, 4U, context);
    const std::uint32_t value = (static_cast<std::uint32_t>(in[offset]) << 24U) |
                                (static_cast<std::uint32_t>(in[offset + 1U]) << 16U) |
                                (static_cast<std::uint32_t>(in[offset + 2U]) << 8U) |
                                static_cast<std::uint32_t>(in[offset + 3U]);
    offset += 4U;
    return value;
}

inline std::uint64_t binary_read_u64(const Bytes& in, std::size_t& offset,
                                     std::string_view context)
{
    binary_require_remaining(in, offset, 8U, context);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8U; ++i)
    {
        value = (value << 8U) | in[offset + i];
    }
    offset += 8U;
    return value;
}

inline float binary_read_f32(const Bytes& in, std::size_t& offset,
                             std::string_view context)
{
    const std::uint32_t bits = binary_read_u32(in, offset, context);
    float value = 0.0F;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline void binary_write_bytes_u16(Bytes& out, const Bytes& bytes)
{
    if (bytes.size() > std::numeric_limits<std::uint16_t>::max())
    {
        throw PacketError("byte field is too large for uint16 length");
    }
    binary_write_u16(out, static_cast<std::uint16_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

inline Bytes binary_read_bytes_u16(const Bytes& in, std::size_t& offset,
                                   std::string_view context)
{
    const std::uint16_t len = binary_read_u16(in, offset, context);
    if (offset > in.size() || in.size() - offset < len)
    {
        throw PacketError(std::string(context) +
                          " length-prefixed byte field exceeds payload");
    }
    Bytes out(in.begin() + static_cast<std::ptrdiff_t>(offset),
              in.begin() + static_cast<std::ptrdiff_t>(offset + len));
    offset += len;
    return out;
}

inline void binary_require_end(const Bytes& in, std::size_t offset,
                               std::string_view context)
{
    if (offset != in.size())
    {
        throw PacketError(std::string(context) + " has trailing bytes");
    }
}
} // namespace cyber::protocol::detail
