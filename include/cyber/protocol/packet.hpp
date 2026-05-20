#pragma once

#include "cyber/shared/types.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace cyber
{
// 网络 payload 的统一字节数组类型。
using Bytes = std::vector<std::uint8_t>;

// 报文序列化、解析或协议字段非法时抛出的异常。
class PacketError : public std::runtime_error
{
public:
    // 保存协议错误说明。
    explicit PacketError(const std::string& message);
};

// 完整网络报文，header 字段与可变长 payload 一起在 TCP 上传输。
struct Packet
{
    MsgType msg_type = MsgType::error;
    EntityId src = EntityId::unknown;
    EntityId dst = EntityId::unknown;
    // payload_len 在序列化时由 payload.size() 写入固定头。
    std::uint32_t reserved = kDefaultReserved;
    Bytes payload;
};

// 固定 11 字节报文头的结构化表示，UI 报文头可视化直接使用它。
struct PacketHeader
{
    MsgType msg_type = MsgType::error;
    EntityId src = EntityId::unknown;
    EntityId dst = EntityId::unknown;
    std::uint32_t payload_len = 0;
    std::uint32_t reserved = kDefaultReserved;
};

// 构造一个完整 Packet，payload_len 在序列化时由 payload.size() 计算。
Packet make_packet(MsgType msg_type, EntityId src, EntityId dst, Bytes payload = {},
                   std::uint32_t reserved = kDefaultReserved);
// 从 Packet 中抽取固定头字段。
PacketHeader packet_header(const Packet& packet);
// 从 11 字节网络头解析 PacketHeader。
PacketHeader parse_packet_header(const Bytes& bytes);

// 将 Packet 序列化为 TCP 发送的字节流。
Bytes serialize_packet(const Packet& packet);
// 从 TCP 字节流解析完整 Packet。
Packet parse_packet(const Bytes& bytes);

// 构造 MSG_ERROR 的 payload，包含错误码和错误文本。
Bytes make_error_payload(ErrorCode code, const std::string& message);
// 从 MSG_ERROR payload 读取错误码。
ErrorCode parse_error_code(const Bytes& payload);
// 从 MSG_ERROR payload 读取错误文本。
std::string parse_error_message(const Bytes& payload);

// 构造 MSG_APP payload，第一字节是 AppCode，后面是应用层数据。
Bytes make_app_payload(AppCode code, const Bytes& app_payload);
// 从 MSG_APP payload 读取 AppCode。
AppCode parse_app_code(const Bytes& payload);
// 从 MSG_APP payload 去掉 AppCode 后取出应用层数据。
Bytes parse_app_payload(const Bytes& payload);

// 将字节数组转为十六进制字符串，主要服务日志和 UI。
std::string bytes_to_hex(const Bytes& bytes);
// 将十六进制字符串解析回字节数组。
Bytes bytes_from_hex(const std::string& hex);
} // namespace cyber
