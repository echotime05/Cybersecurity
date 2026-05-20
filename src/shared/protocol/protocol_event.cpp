#include "cyber/protocol/protocol_event.hpp"

#include "cyber/shared/crypto.hpp"
#include "cyber/shared/logger.hpp"
#include "cyber/shared/runtime_paths.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace cyber
{
namespace
{
// 将 EntityId 转换成协议日志里使用的角色名。
std::string entity_label(EntityId id)
{
    return std::string(to_string(id));
}

// 将协议方向转换成 SEND/RECV 文本。
std::string direction_label(ProtocolDirection direction)
{
    return direction == ProtocolDirection::send ? "SEND" : "RECV";
}

// 将协议方向转换成 Logger 事件名。
std::string event_label(ProtocolDirection direction)
{
    return direction == ProtocolDirection::send ? "PACKET_SEND" : "PACKET_RECV";
}

// 将 1 字节数值格式化为 UI 展示用十六进制。
std::string hex_u8(std::uint8_t value)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<int>(value);
    return oss.str();
}

// 将 4 字节数值格式化为 UI 展示用十六进制。
std::string hex_u32(std::uint32_t value)
{
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
        << value;
    return oss.str();
}

// 根据报文类型给 UI 分类：app、error 或 kerberos。
std::string category_for_packet(const Packet& packet)
{
    if (packet.msg_type == MsgType::app)
    {
        return "app";
    }
    if (packet.msg_type == MsgType::error)
    {
        return "error";
    }
    return "kerberos";
}

// 在调用方没有提供说明时，为报文生成默认消息名。
std::string default_message_for_packet(const Packet& packet)
{
    if (packet.msg_type == MsgType::app)
    {
        if (!packet.payload.empty() && packet.payload.size() % 8U != 0U)
        {
            try
            {
                return protocol_app_message(parse_app_code(packet.payload));
            }
            catch (const std::exception&)
            {
            }
        }
        return "MSG_APP";
    }
    if (packet.msg_type == MsgType::error && !packet.payload.empty())
    {
        try
        {
            return std::string("MSG_ERROR.") + std::string(to_string(parse_error_code(packet.payload)));
        }
        catch (const std::exception&)
        {
            return "MSG_ERROR";
        }
    }
    return std::string(to_string(packet.msg_type));
}

// 根据 SEND/RECV 判断本进程角色，决定写入哪个 protocol_events 文件。
EntityId role_for_direction(ProtocolDirection direction, const Packet& packet)
{
    return direction == ProtocolDirection::send ? packet.src : packet.dst;
}

// 将角色转换成 protocol_events 文件名前缀。
std::string role_file_stem(EntityId role)
{
    switch (role)
    {
    case EntityId::client1:
        return "client_01";
    case EntityId::client2:
        return "client_02";
    case EntityId::client3:
        return "client_03";
    case EntityId::client4:
        return "client_04";
    case EntityId::as:
        return "as";
    case EntityId::tgs:
        return "tgs";
    case EntityId::v:
        return "v";
    default:
        return "unknown";
    }
}

// 前置声明全局日志根目录访问函数，event_log_path 会调用它。
std::filesystem::path& protocol_event_log_root();

// 转义 JSON 字符串内容。
std::string json_escape(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char ch : value)
    {
        switch (ch)
        {
        case '"':
        case '\\':
            out.push_back('\\');
            out.push_back(ch);
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

// 解析协议日志中 key=value 形式的字段。
std::map<std::string, std::string> parse_key_values(const std::string& text)
{
    std::map<std::string, std::string> values;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token)
    {
        const std::size_t eq = token.find('=');
        if (eq == std::string::npos || eq == 0U)
        {
            continue;
        }
        values[token.substr(0, eq)] = token.substr(eq + 1U);
    }
    return values;
}

// 读取必填 key，不存在时抛异常。
std::string required_value(const std::map<std::string, std::string>& values,
                           const char* key)
{
    const auto it = values.find(key);
    if (it == values.end())
    {
        throw std::runtime_error(std::string("missing protocol event key: ") + key);
    }
    return it->second;
}

// 读取可选 key，不存在时返回空字符串。
std::string optional_value(const std::map<std::string, std::string>& values,
                           const std::string& key)
{
    const auto it = values.find(key);
    return it == values.end() ? std::string() : it->second;
}

// 生成当前进程、当前角色的协议事件日志文件路径。
std::filesystem::path event_log_path(EntityId role)
{
    const DWORD pid = GetCurrentProcessId();
    return protocol_event_log_root() / "protocol_events" /
           (role_file_stem(role) + "_" + std::to_string(pid) + ".txt");
}

// 返回保护协议日志器表的全局 mutex。
std::mutex& logger_mutex()
{
    static std::mutex mutex;
    return mutex;
}

// 返回按角色缓存的协议日志器表。
std::map<EntityId, std::unique_ptr<Logger>>& protocol_loggers()
{
    static std::map<EntityId, std::unique_ptr<Logger>> loggers;
    return loggers;
}

// 返回协议事件日志根目录的全局引用。
std::filesystem::path& protocol_event_log_root()
{
    static std::filesystem::path root = default_log_root();
    return root;
}
} // namespace

// 生成 HH:MM:SS.mmm 格式的协议事件时间戳。
std::string protocol_timestamp_now()
{
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &time);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << local.tm_hour << ':' << std::setw(2)
        << local.tm_min << ':' << std::setw(2) << local.tm_sec << '.' << std::setw(3)
        << millis.count();
    return oss.str();
}

// 将 AppCode 格式化为 MSG_APP.xxx。
std::string protocol_app_message(AppCode code)
{
    return std::string("MSG_APP.") + std::string(to_string(code));
}

// 设置协议事件日志根目录，并清空旧日志器缓存。
void set_protocol_event_log_root(std::filesystem::path root)
{
    std::lock_guard<std::mutex> lock(logger_mutex());
    protocol_event_log_root() = std::move(root);
    protocol_loggers().clear();
}

// 生成协议事件日志行，payload 使用默认视图。
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override)
{
    return format_protocol_event_message(direction, packet, message_override, timestamp_override,
                                         {});
}

// 生成协议事件日志行，包含固定头、原始 packet、payload 明文/密文和字段视图。
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override,
                                          const ProtocolPayloadView& payload_view)
{
    const PacketHeader header = packet_header(packet);
    const std::string timestamp =
        timestamp_override.empty() ? protocol_timestamp_now() : std::string(timestamp_override);
    const std::string message =
        message_override.empty() ? default_message_for_packet(packet)
                                 : std::string(message_override);

    std::ostringstream oss;
    oss << "ts=" << timestamp << " direction=" << direction_label(direction)
        << " endpoint=" << entity_label(packet.src) << "->" << entity_label(packet.dst)
        << " message=" << message << " category=" << category_for_packet(packet)
        << " msg_type=" << to_string(header.msg_type)
        << " msg_type_raw=" << hex_u8(static_cast<std::uint8_t>(header.msg_type))
        << " src=" << entity_label(header.src)
        << " src_raw=" << hex_u8(static_cast<std::uint8_t>(header.src))
        << " dst=" << entity_label(header.dst)
        << " dst_raw=" << hex_u8(static_cast<std::uint8_t>(header.dst))
        << " payload_len=" << header.payload_len
        << " payload_len_raw=" << hex_u32(header.payload_len)
        << " reserved=" << header.reserved << " reserved_raw=" << hex_u32(header.reserved)
        << " packet_hex=" << bytes_to_hex(serialize_packet(packet))
        << " payload_hex=" << bytes_to_hex(packet.payload);
    if (!payload_view.plain_hex.empty())
    {
        oss << " payload_plain_hex=" << payload_view.plain_hex;
    }
    if (!payload_view.encrypted_hex.empty())
    {
        oss << " payload_encrypted_hex=" << payload_view.encrypted_hex;
    }
    if (!payload_view.fields.empty())
    {
        oss << " field_count=" << payload_view.fields.size();
        for (std::size_t i = 0; i < payload_view.fields.size(); ++i)
        {
            const ProtocolPayloadView::Field& field = payload_view.fields[i];
            oss << " field" << i << "_name=" << field.name;
            if (!field.plain_hex.empty())
            {
                oss << " field" << i << "_plain_hex=" << field.plain_hex;
            }
            if (!field.encrypted_hex.empty())
            {
                oss << " field" << i << "_encrypted_hex=" << field.encrypted_hex;
            }
        }
    }
    return oss.str();
}

// 从 Logger 写出的一行 protocol_events 文本恢复结构化 ProtocolEvent。
ProtocolEvent parse_protocol_event_line(const std::string& line)
{
    const std::size_t role_start = line.find('[');
    const std::size_t role_end = line.find(']', role_start + 1U);
    const std::size_t thread_start = line.find('[', role_end + 1U);
    const std::size_t thread_end = line.find(']', thread_start + 1U);
    const std::size_t event_start = line.find('[', thread_end + 1U);
    const std::size_t event_end = line.find(']', event_start + 1U);
    if (role_start != 0U || role_end == std::string::npos ||
        thread_start == std::string::npos || thread_end == std::string::npos ||
        event_start == std::string::npos || event_end == std::string::npos ||
        event_end + 2U > line.size())
    {
        throw std::runtime_error("invalid protocol event line");
    }

    const std::string logger_event = line.substr(event_start + 1U, event_end - event_start - 1U);
    if (logger_event != "PACKET_SEND" && logger_event != "PACKET_RECV")
    {
        throw std::runtime_error("unsupported protocol event kind");
    }

    const std::map<std::string, std::string> values =
        parse_key_values(line.substr(event_end + 2U));

    ProtocolEvent event;
    event.role = line.substr(role_start + 1U, role_end - role_start - 1U);
    event.timestamp = required_value(values, "ts");
    event.direction = required_value(values, "direction");
    event.endpoint = required_value(values, "endpoint");
    event.message = required_value(values, "message");
    event.category = required_value(values, "category");
    event.header.msg_type = {required_value(values, "msg_type"),
                             required_value(values, "msg_type_raw")};
    event.header.src = {required_value(values, "src"), required_value(values, "src_raw")};
    event.header.dst = {required_value(values, "dst"), required_value(values, "dst_raw")};
    event.header.payload_len = {required_value(values, "payload_len"),
                                required_value(values, "payload_len_raw")};
    event.header.reserved = {required_value(values, "reserved"),
                             required_value(values, "reserved_raw")};
    event.packet_hex = optional_value(values, "packet_hex");
    event.payload_hex = required_value(values, "payload_hex");
    if (const auto it = values.find("payload_plain_hex"); it != values.end())
    {
        event.payload_plain_hex = it->second;
    }
    if (const auto it = values.find("payload_encrypted_hex"); it != values.end())
    {
        event.payload_encrypted_hex = it->second;
    }
    if (const auto it = values.find("field_count"); it != values.end())
    {
        const std::size_t count = static_cast<std::size_t>(std::stoul(it->second));
        for (std::size_t i = 0; i < count; ++i)
        {
            ProtocolPayloadView::Field field;
            field.name = required_value(values, ("field" + std::to_string(i) + "_name").c_str());
            field.plain_hex = optional_value(values, "field" + std::to_string(i) + "_plain_hex");
            field.encrypted_hex =
                optional_value(values, "field" + std::to_string(i) + "_encrypted_hex");
            event.payload_fields.push_back(std::move(field));
        }
    }
    return event;
}

// 将 ProtocolEvent 转换成 Monitor 推给 Web UI 的 JSON。
std::string protocol_event_json(const ProtocolEvent& event, std::uint64_t id)
{
    auto field_json = [](const ProtocolFieldView& field) {
        return std::string("{\"label\":\"") + json_escape(field.label) + "\",\"raw\":\"" +
               json_escape(field.raw) + "\"}";
    };

    auto payload_field_json = [](const ProtocolPayloadView::Field& field) {
        return std::string("{\"name\":\"") + json_escape(field.name) + "\",\"plainHex\":\"" +
               json_escape(field.plain_hex) + "\",\"encryptedHex\":\"" +
               json_escape(field.encrypted_hex) + "\"}";
    };

    std::ostringstream oss;
    oss << "{\"type\":\"protocolEvent\",\"id\":" << id << ",\"timestamp\":\""
        << json_escape(event.timestamp) << "\",\"role\":\"" << json_escape(event.role)
        << "\",\"direction\":\"" << json_escape(event.direction) << "\",\"endpoint\":\""
        << json_escape(event.endpoint) << "\",\"message\":\"" << json_escape(event.message)
        << "\",\"category\":\"" << json_escape(event.category) << "\",\"header\":{"
        << "\"msgType\":" << field_json(event.header.msg_type)
        << ",\"src\":" << field_json(event.header.src)
        << ",\"dst\":" << field_json(event.header.dst)
        << ",\"payloadLen\":" << field_json(event.header.payload_len)
        << ",\"reserved\":" << field_json(event.header.reserved) << "},\"packetHex\":\""
        << json_escape(event.packet_hex) << "\",\"payloadHex\":\""
        << json_escape(event.payload_hex) << "\",\"payloadPlainHex\":\""
        << json_escape(event.payload_plain_hex) << "\",\"payloadEncryptedHex\":\""
        << json_escape(event.payload_encrypted_hex) << "\",\"payloadFields\":[";
    for (std::size_t i = 0; i < event.payload_fields.size(); ++i)
    {
        if (i != 0U)
        {
            oss << ',';
        }
        oss << payload_field_json(event.payload_fields[i]);
    }
    oss << "]}";
    return oss.str();
}

// 写入协议事件日志，payload 使用默认原始视图。
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override)
{
    write_protocol_event(direction, packet, message_override, {});
}

// 写入协议事件日志，附带调用方提供的 payload 明文/密文/字段视图。
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override,
                          const ProtocolPayloadView& payload_view)
{
    const EntityId role = role_for_direction(direction, packet);
    if (!is_known(role))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(logger_mutex());
    auto& loggers = protocol_loggers();
    auto it = loggers.find(role);
    if (it == loggers.end())
    {
        it = loggers.emplace(role, std::make_unique<Logger>(event_log_path(role))).first;
    }
    it->second->write(entity_label(role), "ProtocolMonitor", event_label(direction),
                      format_protocol_event_message(direction, packet, message_override, {},
                                                    payload_view));
}
} // namespace cyber
