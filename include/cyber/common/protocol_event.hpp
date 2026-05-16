#pragma once

#include "cyber/common/packet.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace cyber
{
enum class ProtocolDirection
{
    send,
    recv
};

struct ProtocolFieldView
{
    std::string label;
    std::string raw;
};

struct ProtocolHeaderView
{
    ProtocolFieldView msg_type;
    ProtocolFieldView src;
    ProtocolFieldView dst;
    ProtocolFieldView payload_len;
    ProtocolFieldView reserved;
};

struct ProtocolEvent
{
    std::string role;
    std::string timestamp;
    std::string direction;
    std::string endpoint;
    std::string message;
    std::string category;
    ProtocolHeaderView header;
    std::string payload_hex;
};

std::string protocol_timestamp_now();
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override = {},
                                          std::string_view timestamp_override = {});
ProtocolEvent parse_protocol_event_line(const std::string& line);
std::string protocol_event_json(const ProtocolEvent& event, std::uint64_t id);
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override = {});
std::string protocol_app_message(AppCode code);
} // namespace cyber
