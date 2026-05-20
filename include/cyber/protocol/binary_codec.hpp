#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace cyber::protocol::detail
{
// 检查二进制解析时剩余字节是否足够，不够则抛 PacketError。
inline void binary_require_remaining(const Bytes& in, std::size_t offset,
                                     std::size_t count, std::string_view context)
{
    if (offset > in.size() || in.size() - offset < count)
    {
        throw PacketError(std::string(context) + " is too short");
    }
}

// 按大端序写入 16 位无符号整数。
inline void binary_write_u16(Bytes& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

// 按大端序写入 32 位无符号整数。
inline void binary_write_u32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

// 按大端序写入 64 位无符号整数。
inline void binary_write_u64(Bytes& out, std::uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

// 将 float32 按原始 IEEE754 位写入网络字节。
inline void binary_write_f32(Bytes& out, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    binary_write_u32(out, bits);
}

// 按大端序读取 16 位无符号整数，并推进 offset。
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

// 按大端序读取 32 位无符号整数，并推进 offset。
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

// 按大端序读取 64 位无符号整数，并推进 offset。
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

// 读取 float32 的原始位并还原为 float。
inline float binary_read_f32(const Bytes& in, std::size_t& offset,
                             std::string_view context)
{
    const std::uint32_t bits = binary_read_u32(in, offset, context);
    float value = 0.0F;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// 写入 uint16 长度前缀和对应字节串。
inline void binary_write_bytes_u16(Bytes& out, const Bytes& bytes)
{
    if (bytes.size() > std::numeric_limits<std::uint16_t>::max())
    {
        throw PacketError("byte field is too large for uint16 length");
    }
    binary_write_u16(out, static_cast<std::uint16_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

// 读取 uint16 长度前缀的字节串。
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

// 要求 payload 已被完整消费，防止格式错位仍被接受。
inline void binary_require_end(const Bytes& in, std::size_t offset,
                               std::string_view context)
{
    if (offset != in.size())
    {
        throw PacketError(std::string(context) + " has trailing bytes");
    }
}
} // namespace cyber::protocol::detail
