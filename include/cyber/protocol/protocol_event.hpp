#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cyber
{
// 协议可视化日志中的方向，表示本进程发送或接收一个报文。
enum class ProtocolDirection
{
    send,
    recv
};

// UI 中一个固定头字段的展示值，label 是语义名，raw 是十六进制原始值。
struct ProtocolFieldView
{
    std::string label;
    std::string raw;
};

// 固定 11 字节 Header 的结构化展示模型。
struct ProtocolHeaderView
{
    ProtocolFieldView msg_type;
    ProtocolFieldView src;
    ProtocolFieldView dst;
    ProtocolFieldView payload_len;
    ProtocolFieldView reserved;
};

// Payload 的结构化展示模型，可同时携带明文、密文和字段级十六进制。
struct ProtocolPayloadView
{
    std::string plain_hex;
    std::string encrypted_hex;
    // Payload 内一个结构化字段，支持明文字节和密文字节对照展示。
    struct Field
    {
        std::string name;
        std::string plain_hex;
        std::string encrypted_hex;
    };
    std::vector<Field> fields;
};

// monitor 读取到的一条协议事件，最终会转换成 Web UI 使用的 JSON。
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

// 生成协议日志使用的当前时间戳字符串。
std::string protocol_timestamp_now();
// 生成一行协议事件文本，payload 按报文原始内容展示。
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override = {},
                                          std::string_view timestamp_override = {});
// 生成一行协议事件文本，并附加调用方提供的 payload 结构化视图。
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override,
                                          const ProtocolPayloadView& payload_view);
// 从 protocol_events/*.txt 的一行文本恢复结构化事件。
ProtocolEvent parse_protocol_event_line(const std::string& line);
// 将结构化事件转成 monitor WebSocket 推送给 UI 的 JSON。
std::string protocol_event_json(const ProtocolEvent& event, std::uint64_t id);
// 写入协议事件日志，payload 按原始报文展示。
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override = {});
// 写入协议事件日志，并记录 payload 明文/密文/字段视图。
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override,
                          const ProtocolPayloadView& payload_view);
// 设置协议事件日志根目录，测试和运行脚本会用它切换 logs/protocol_events。
void set_protocol_event_log_root(std::filesystem::path root);
// 把 AppCode 转成 UI 左侧列表展示的 MSG_APP 后缀。
std::string protocol_app_message(AppCode code);
} // namespace cyber
