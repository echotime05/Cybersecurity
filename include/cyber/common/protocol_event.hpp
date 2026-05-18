#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

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

struct ProtocolPayloadView
{
    std::string plain_hex;
    std::string encrypted_hex;
    struct Field
    {
        std::string name;
        std::string plain_hex;
        std::string encrypted_hex;
    };
    std::vector<Field> fields;
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
    std::string packet_hex;
    std::string payload_hex;
    std::string payload_plain_hex;
    std::string payload_encrypted_hex;
    std::vector<ProtocolPayloadView::Field> payload_fields;
};

std::string protocol_timestamp_now();
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override = {},
                                          std::string_view timestamp_override = {});
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override,
                                          const ProtocolPayloadView& payload_view);
ProtocolEvent parse_protocol_event_line(const std::string& line);
std::string protocol_event_json(const ProtocolEvent& event, std::uint64_t id);
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override = {});
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override,
                          const ProtocolPayloadView& payload_view);
void set_protocol_event_log_root(std::filesystem::path root);
std::string protocol_app_message(AppCode code);
} // namespace cyber
