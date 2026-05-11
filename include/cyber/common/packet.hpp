#pragma once

#include "cyber/common/types.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace cyber
{
using Bytes = std::vector<std::uint8_t>;

class PacketError : public std::runtime_error
{
public:
    explicit PacketError(const std::string& message);
};

struct Packet
{
    MsgType msg_type = MsgType::error;
    EntityId src = EntityId::unknown;
    EntityId dst = EntityId::unknown;
    // payload_len is encoded from payload.size() during serialization.
    std::uint32_t reserved = kDefaultReserved;
    Bytes payload;
};

Bytes serialize_packet(const Packet& packet);
Packet parse_packet(const Bytes& bytes);

Bytes make_error_payload(ErrorCode code, const std::string& message);
ErrorCode parse_error_code(const Bytes& payload);

Bytes make_app_payload(AppCode code, const Bytes& app_payload);
AppCode parse_app_code(const Bytes& payload);
Bytes parse_app_payload(const Bytes& payload);

std::string bytes_to_hex(const Bytes& bytes);
Bytes bytes_from_hex(const std::string& hex);
} // namespace cyber
