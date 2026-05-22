# packet.cpp 与 protocol_event.cpp 函数详解

对应文件：

```text
src/shared/protocol/packet.cpp
src/shared/protocol/protocol_event.cpp
```

这两个文件都属于协议底层，但负责的事情不一样。

```text
packet.cpp
  -> 负责 Packet 这个网络包怎么创建、怎么转成字节、怎么从字节解析回来。
  -> 也负责 error/app payload 和 hex 字符串转换。

protocol_event.cpp
  -> 负责把收发的 Packet 记录成协议事件日志。
  -> 也负责把日志行解析回来，再转成前端 monitor 能用的 JSON。
```

一句话：

```text
packet.cpp 管“包本身”。
protocol_event.cpp 管“这个包怎么被记录和展示”。
```

## 1. 先理解 Packet 是什么

`Packet` 定义在：

```text
include/cyber/protocol/packet.hpp
```

结构：

```cpp
struct Packet
{
    MsgType msg_type;
    EntityId src;
    EntityId dst;
    std::uint32_t reserved;
    Bytes payload;
};
```

人话：

```text
Packet 就是一封网络信。
msg_type 表示这封信是什么类型，比如 MSG_AS_REQ。
src 表示谁发的。
dst 表示发给谁。
reserved 是保留字段。
payload 是真正的正文内容。
```

网络上传输时，Packet 会变成：

```text
固定 11 字节包头 + payload
```

包头格式：

```text
第 0 字节      msg_type
第 1 字节      src
第 2 字节      dst
第 3-6 字节    payload_len，大端序 u32
第 7-10 字节   reserved，大端序 u32
后面           payload
```

## 2. packet.cpp 整体负责什么

`packet.cpp` 主要做四类事情：

```text
1. Packet 和 PacketHeader 的构造
   make_packet()
   packet_header()

2. Packet 和网络字节互转
   serialize_packet()
   parse_packet()
   parse_packet_header()

3. 特殊 payload 编码
   make_error_payload()
   parse_error_code()
   parse_error_message()
   make_app_payload()
   parse_app_code()
   parse_app_payload()

4. bytes 和 hex 字符串互转
   bytes_to_hex()
   bytes_from_hex()
```

## 3. PacketError::PacketError()

代码：

```cpp
PacketError::PacketError(const std::string& message) : std::runtime_error(message)
{
}
```

作用：

```text
定义 PacketError 这个异常类型的构造函数。
```

人话：

```text
Packet 解析、序列化、payload 格式出问题时，代码会抛 PacketError。
PacketError 本质上是 std::runtime_error，只是名字更具体。
```

例子：

```text
packet 太短
payload 长度不匹配
hex 字符串不合法
error payload 为空
```

这些都会抛 `PacketError`。

## 4. to_byte(MsgType type)

位置：

```text
packet.cpp 匿名 namespace 内部函数
```

代码：

```cpp
std::uint8_t to_byte(MsgType type)
{
    return static_cast<std::uint8_t>(type);
}
```

作用：

```text
把 MsgType 枚举转成 1 字节整数。
```

人话：

```text
网络上传的包头只能放字节。
MsgType::as_req 这种枚举在写入网络字节前，要转成 uint8_t。
```

调用位置：

```text
serialize_packet()
```

## 5. to_byte(EntityId id)

代码：

```cpp
std::uint8_t to_byte(EntityId id)
{
    return static_cast<std::uint8_t>(id);
}
```

作用：

```text
把 EntityId 枚举转成 1 字节整数。
```

人话：

```text
Packet 里的 src/dst 是 EntityId。
写到网络包头时，要转成一个字节。
```

调用位置：

```text
serialize_packet()
```

## 6. write_u32_be()

代码：

```cpp
void write_u32_be(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}
```

作用：

```text
把 32 位整数按大端序写进字节数组。
```

什么是大端序：

```text
高位字节先写，低位字节后写。
```

比如：

```text
0x12345678
```

会写成：

```text
12 34 56 78
```

调用位置：

```text
serialize_packet()
```

用于写：

```text
payload_len
reserved
```

## 7. read_u32_be()

代码：

```cpp
std::uint32_t read_u32_be(const Bytes& bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes.at(offset)) << 24U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 1U)) << 16U) |
           (static_cast<std::uint32_t>(bytes.at(offset + 2U)) << 8U) |
           static_cast<std::uint32_t>(bytes.at(offset + 3U));
}
```

作用：

```text
从字节数组里按大端序读一个 32 位整数。
```

调用位置：

```text
parse_packet_header()
```

用于读：

```text
payload_len
reserved
```

注意：

```text
这里用 bytes.at(...)，如果越界会抛异常。
不过 parse_packet_header() 前面已经检查了最少 11 字节。
```

## 8. hex_value()

代码：

```cpp
int hex_value(char ch)
```

作用：

```text
把一个十六进制字符转成数值。
```

支持：

```text
'0' - '9' -> 0 到 9
'a' - 'f' -> 10 到 15
'A' - 'F' -> 10 到 15
```

如果不是十六进制字符：

```cpp
return -1;
```

调用位置：

```text
bytes_from_hex()
```

人话：

```text
bytes_from_hex() 需要把 "0A" 这种字符串转回字节。
hex_value() 就是把 '0' 和 'A' 分别变成数字。
```

## 9. make_packet()

代码：

```cpp
Packet make_packet(MsgType msg_type, EntityId src, EntityId dst, Bytes payload,
                   std::uint32_t reserved)
{
    Packet packet;
    packet.msg_type = msg_type;
    packet.src = src;
    packet.dst = dst;
    packet.reserved = reserved;
    packet.payload = std::move(payload);
    return packet;
}
```

作用：

```text
创建一个 Packet。
```

人话：

```text
把消息类型、发送者、接收者、payload 填进 Packet 结构体。
```

例子：

```cpp
make_packet(MsgType::as_req, EntityId::client1, EntityId::as, as_build_req(...))
```

意思是：

```text
创建一封 AS_REQ。
发件人是 Client1。
收件人是 AS。
信纸内容是 as_build_req(...) 生成的字节。
```

为什么 `payload = std::move(payload)`：

```text
payload 是字节数组，可能比较大。
std::move 可以把传进来的字节数组直接搬进 packet，减少拷贝。
```

## 10. packet_header()

代码：

```cpp
PacketHeader packet_header(const Packet& packet)
```

作用：

```text
根据 Packet 生成 PacketHeader。
```

它会做两件事：

第一，检查 payload 长度能不能放进 u32：

```cpp
if (packet.payload.size() > std::numeric_limits<std::uint32_t>::max())
{
    throw PacketError("packet payload is too large");
}
```

第二，把 Packet 的字段复制到 header：

```cpp
header.msg_type = packet.msg_type;
header.src = packet.src;
header.dst = packet.dst;
header.payload_len = static_cast<std::uint32_t>(packet.payload.size());
header.reserved = packet.reserved;
```

人话：

```text
Packet 里没有单独存 payload_len，因为 payload_len 可以从 payload.size() 算出来。
真正写网络包头时，需要有 payload_len，所以这里临时生成 PacketHeader。
```

调用位置：

```text
serialize_packet()
format_protocol_event_message()
```

## 11. parse_packet_header()

代码：

```cpp
PacketHeader parse_packet_header(const Bytes& bytes)
```

作用：

```text
从网络字节里解析固定 11 字节包头。
```

先检查长度：

```cpp
if (bytes.size() < kPacketHeaderSize)
{
    throw PacketError("packet is shorter than the fixed header");
}
```

然后按固定位置读：

```cpp
header.msg_type = static_cast<MsgType>(bytes[0]);
header.src = static_cast<EntityId>(bytes[1]);
header.dst = static_cast<EntityId>(bytes[2]);
header.payload_len = read_u32_be(bytes, 3);
header.reserved = read_u32_be(bytes, 7);
```

人话：

```text
收到网络数据后，先看前 11 个字节。
第 0 个字节是消息类型。
第 1 个字节是发送者。
第 2 个字节是接收者。
第 3-6 字节是 payload 长度。
第 7-10 字节是 reserved。
```

调用位置：

```text
parse_packet()
recv_packet_logged()
```

## 12. serialize_packet()

代码：

```cpp
Bytes serialize_packet(const Packet& packet)
```

作用：

```text
把 Packet 结构体转成可以通过 socket 发送的字节数组。
```

流程：

```text
1. packet_header(packet)
   生成包头信息。

2. out.reserve(kPacketHeaderSize + packet.payload.size())
   提前分配空间。

3. 写 msg_type/src/dst。

4. 写 payload_len/reserved。

5. 把 payload 追加到后面。
```

关键代码：

```cpp
out.push_back(to_byte(header.msg_type));
out.push_back(to_byte(header.src));
out.push_back(to_byte(header.dst));
write_u32_be(out, header.payload_len);
write_u32_be(out, header.reserved);
out.insert(out.end(), packet.payload.begin(), packet.payload.end());
```

人话：

```text
发送 Packet 前，必须先把它变成网络字节。
serialize_packet() 就是在做“装箱打包”。
```

调用位置：

```text
send_packet_logged()
format_protocol_event_message() 里生成 packet_hex
```

## 13. parse_packet()

代码：

```cpp
Packet parse_packet(const Bytes& bytes)
```

作用：

```text
把网络字节解析回 Packet 结构体。
```

流程：

```text
1. 检查总长度至少有 11 字节包头。
2. parse_packet_header(bytes) 解析包头。
3. 检查 bytes.size() 是否等于 11 + payload_len。
4. 创建 Packet。
5. 把 header 字段填回 Packet。
6. 把剩下的字节作为 payload。
```

关键检查：

```cpp
if (bytes.size() != kPacketHeaderSize + header.payload_len)
{
    throw PacketError("packet payload length mismatch");
}
```

人话：

```text
包头说 payload 有多长，真实收到的字节就必须刚好这么长。
否则说明包不完整或格式不对。
```

调用位置：

```text
recv_packet_logged()
测试代码
```

## 14. make_error_payload()

代码：

```cpp
Bytes make_error_payload(ErrorCode code, const std::string& message)
```

作用：

```text
构造 MSG_ERROR 的 payload。
```

格式：

```text
第 1 字节：ErrorCode
后面：错误信息字符串
```

代码：

```cpp
payload.push_back(static_cast<std::uint8_t>(code));
payload.insert(payload.end(), message.begin(), message.end());
```

人话：

```text
如果要发一个错误包，payload 里先放错误码，再放错误文字。
```

## 15. parse_error_code()

代码：

```cpp
ErrorCode parse_error_code(const Bytes& payload)
```

作用：

```text
从 MSG_ERROR payload 里读错误码。
```

如果 payload 为空：

```cpp
throw PacketError("error payload is empty");
```

否则：

```cpp
return static_cast<ErrorCode>(payload[0]);
```

人话：

```text
错误 payload 第一个字节就是错误码。
```

## 16. parse_error_message()

代码：

```cpp
std::string parse_error_message(const Bytes& payload)
```

作用：

```text
从 MSG_ERROR payload 里读错误文字。
```

格式：

```text
payload[0] 是错误码。
payload[1..end] 是错误消息。
```

所以返回：

```cpp
return std::string(payload.begin() + 1, payload.end());
```

人话：

```text
跳过第一个错误码字节，把后面的字节当字符串。
```

## 17. make_app_payload()

代码：

```cpp
Bytes make_app_payload(AppCode code, const Bytes& app_payload)
```

作用：

```text
构造 MSG_APP 的 payload。
```

格式：

```text
第 1 字节：AppCode
后面：应用层 payload
```

比如游戏消息：

```text
GAME_MOVE
GAME_SHOOT
GAME_STATE
```

都可以用 AppCode 区分。

## 18. parse_app_code()

代码：

```cpp
AppCode parse_app_code(const Bytes& payload)
```

作用：

```text
从 MSG_APP payload 里读 AppCode。
```

如果 payload 为空：

```cpp
throw PacketError("app payload is empty");
```

否则读取：

```cpp
payload[0]
```

## 19. parse_app_payload()

代码：

```cpp
Bytes parse_app_payload(const Bytes& payload)
```

作用：

```text
从 MSG_APP payload 里取出真正的应用层正文。
```

格式：

```text
payload[0] 是 AppCode。
payload[1..end] 是真正内容。
```

返回：

```cpp
return Bytes(payload.begin() + 1, payload.end());
```

## 20. bytes_to_hex()

代码：

```cpp
std::string bytes_to_hex(const Bytes& bytes)
```

作用：

```text
把字节数组转成十六进制字符串。
```

比如：

```text
Bytes{0x01, 0xAB, 0x0F}
```

会变成：

```text
01ab0f
```

关键代码：

```cpp
oss << std::hex << std::setfill('0');
for (std::uint8_t byte : bytes)
{
    oss << std::setw(2) << static_cast<int>(byte);
}
```

调用位置：

```text
protocol_event.cpp 生成 payload_hex / packet_hex
AS/TGS/Client 生成明文密文展示
测试代码
```

人话：

```text
二进制字节不好直接打印，所以日志里统一转成 hex 字符串。
```

## 21. bytes_from_hex()

代码：

```cpp
Bytes bytes_from_hex(const std::string& hex)
```

作用：

```text
把十六进制字符串转回字节数组。
```

流程：

```text
1. 去掉空白字符。
2. 如果前面有 0x 或 0X，就去掉。
3. 如果 hex 长度是奇数，就前面补一个 0。
4. 每两个字符转成一个字节。
5. 如果遇到非法 hex 字符，抛 PacketError。
```

例子：

```text
"0x0A FF"
```

去空白和 0x 后：

```text
"0AFF"
```

转成：

```text
Bytes{0x0A, 0xFF}
```

调用位置：

```text
测试、解析工具、可能的调试场景。
```

## 22. protocol_event.cpp 整体负责什么

`protocol_event.cpp` 做的事情是：

```text
1. Packet 收发时，生成一行可读的协议日志。
2. 把日志写到 _generated/logs/protocol_events/*.txt。
3. monitor 再读取这些日志。
4. 把日志解析成 ProtocolEvent。
5. 转成 JSON 推给网页。
```

一条日志大概长这样：

```text
[AS][ProtocolMonitor][PACKET_SEND] ts=... direction=SEND endpoint=AS->Client1 message=MSG_AS_REP ...
```

## 23. entity_label()

代码：

```cpp
std::string entity_label(EntityId id)
{
    return std::string(to_string(id));
}
```

作用：

```text
把 EntityId 转成人能看的名字。
```

比如：

```text
EntityId::as -> "AS"
EntityId::client1 -> "Client1"
```

调用位置：

```text
format_protocol_event_message()
write_protocol_event()
```

## 24. direction_label()

代码：

```cpp
std::string direction_label(ProtocolDirection direction)
{
    return direction == ProtocolDirection::send ? "SEND" : "RECV";
}
```

作用：

```text
把发送/接收方向转成日志里的字符串。
```

结果：

```text
send -> SEND
recv -> RECV
```

## 25. event_label()

代码：

```cpp
std::string event_label(ProtocolDirection direction)
{
    return direction == ProtocolDirection::send ? "PACKET_SEND" : "PACKET_RECV";
}
```

作用：

```text
决定 Logger 事件名。
```

日志行开头会有：

```text
[PACKET_SEND]
```

或：

```text
[PACKET_RECV]
```

## 26. hex_u8()

代码：

```cpp
std::string hex_u8(std::uint8_t value)
```

作用：

```text
把 1 字节数值格式化成 0xXX。
```

比如：

```text
0x01
0x11
0xFF
```

用于日志字段：

```text
msg_type_raw
src_raw
dst_raw
```

## 27. hex_u32()

代码：

```cpp
std::string hex_u32(std::uint32_t value)
```

作用：

```text
把 32 位整数格式化成 0xXXXXXXXX。
```

比如：

```text
0x0000000A
```

用于日志字段：

```text
payload_len_raw
reserved_raw
```

## 28. category_for_packet()

代码：

```cpp
std::string category_for_packet(const Packet& packet)
```

作用：

```text
给 Packet 分类。
```

规则：

```text
MsgType::app   -> "app"
MsgType::error -> "error"
其他           -> "kerberos"
```

人话：

```text
AS_REQ、AS_REP、TGS_REQ 这类都归 kerberos。
游戏消息归 app。
错误消息归 error。
```

前端 monitor 可以根据 category 用不同样式显示。

## 29. default_message_for_packet()

代码：

```cpp
std::string default_message_for_packet(const Packet& packet)
```

作用：

```text
如果调用方没有指定 message，就根据 Packet 自动生成 message 名字。
```

规则一：如果是 MSG_APP：

```text
如果 payload 看起来不是纯加密块，并且能解析 AppCode，
就返回 MSG_APP.GAME_MOVE 之类。
否则返回 MSG_APP。
```

代码里有一段：

```cpp
if (!packet.payload.empty() && packet.payload.size() % 8U != 0U)
```

这个判断的含义：

```text
有些加密后的 app payload 长度可能刚好是 8 的倍数，不适合直接 parse_app_code。
如果不像加密块，就尝试解析 AppCode。
```

规则二：如果是 MSG_ERROR：

```text
尝试解析 ErrorCode，返回 MSG_ERROR.ERR_xxx。
失败就返回 MSG_ERROR。
```

规则三：其他：

```text
直接返回 to_string(packet.msg_type)。
比如 MSG_AS_REQ、MSG_AS_REP。
```

## 30. role_for_direction()

代码：

```cpp
EntityId role_for_direction(ProtocolDirection direction, const Packet& packet)
{
    return direction == ProtocolDirection::send ? packet.src : packet.dst;
}
```

作用：

```text
决定这条日志应该写到哪个角色的日志文件里。
```

如果是发送日志：

```text
role = packet.src
```

因为发送方是当前角色。

如果是接收日志：

```text
role = packet.dst
```

因为接收方是当前角色。

例子：

```text
Client1 -> AS 的 AS_REQ
Client 发送时：SEND，role 是 Client1。
AS 接收时：RECV，role 是 AS。
```

## 31. role_file_stem()

代码：

```cpp
std::string role_file_stem(EntityId role)
```

作用：

```text
根据角色 ID 决定日志文件名前缀。
```

映射：

```text
client1 -> client_01
client2 -> client_02
client3 -> client_03
client4 -> client_04
as      -> as
tgs     -> tgs
v       -> v
其他    -> unknown
```

和日志文件关系：

```text
as_1234.txt
client_01_1234.txt
```

## 32. protocol_event_log_root() 前置声明

代码：

```cpp
std::filesystem::path& protocol_event_log_root();
```

作用：

```text
提前声明后面会定义的函数。
```

为什么需要：

```text
event_log_path() 在前面用到了 protocol_event_log_root()。
但 protocol_event_log_root() 的定义在后面，所以这里先声明。
```

## 33. json_escape()

代码：

```cpp
std::string json_escape(const std::string& value)
```

作用：

```text
把字符串处理成安全的 JSON 字符串内容。
```

它会处理：

```text
"  -> \"
\  -> \\
\n -> \\n
\r -> \\r
\t -> \\t
```

调用位置：

```text
protocol_event_json()
```

人话：

```text
日志里的字符串要发给前端，不能直接拼 JSON。
否则遇到引号、反斜杠、换行会破坏 JSON 格式。
```

## 34. parse_key_values()

代码：

```cpp
std::map<std::string, std::string> parse_key_values(const std::string& text)
```

作用：

```text
把日志消息后面的 key=value 字符串解析成 map。
```

比如：

```text
ts=12:00:00.001 direction=SEND message=MSG_AS_REQ
```

解析成：

```text
values["ts"] = "12:00:00.001"
values["direction"] = "SEND"
values["message"] = "MSG_AS_REQ"
```

实现方式：

```text
用 istringstream 按空格切 token。
每个 token 找 '='。
左边当 key，右边当 value。
```

注意：

```text
这种解析方式要求 value 里不能有空格。
所以日志里的字段都设计成无空格字符串或 hex。
```

## 35. required_value()

代码：

```cpp
std::string required_value(const std::map<std::string, std::string>& values,
                           const char* key)
```

作用：

```text
从 map 里取必填字段。
```

如果字段不存在：

```cpp
throw std::runtime_error("missing protocol event key: " + key);
```

调用位置：

```text
parse_protocol_event_line()
```

人话：

```text
解析日志行时，有些字段必须有。
比如 ts、direction、message、payload_hex。
缺了就说明日志格式不对。
```

## 36. optional_value()

代码：

```cpp
std::string optional_value(const std::map<std::string, std::string>& values,
                           const std::string& key)
```

作用：

```text
从 map 里取可选字段。
```

如果没有：

```cpp
return std::string();
```

调用位置：

```text
parse_protocol_event_line()
```

可选字段例子：

```text
packet_hex
payload_plain_hex
payload_encrypted_hex
field0_plain_hex
field0_encrypted_hex
```

## 37. event_log_path()

代码：

```cpp
std::filesystem::path event_log_path(EntityId role)
```

作用：

```text
生成某个角色对应的协议事件日志文件路径。
```

代码：

```cpp
const DWORD pid = GetCurrentProcessId();
return protocol_event_log_root() / "protocol_events" /
       (role_file_stem(role) + "_" + std::to_string(pid) + ".txt");
```

人话：

```text
日志目录在 protocol_event_log_root()/protocol_events。
文件名由角色名和进程 pid 组成。
```

例子：

```text
_generated/logs/protocol_events/as_14016.txt
_generated/logs/protocol_events/client_01_8480.txt
```

为什么带 pid：

```text
避免多个进程同角色时写到同一个文件。
```

## 38. logger_mutex()

代码：

```cpp
std::mutex& logger_mutex()
{
    static std::mutex mutex;
    return mutex;
}
```

作用：

```text
提供一个全局 mutex，保护协议日志相关全局状态。
```

保护对象：

```text
protocol_loggers()
protocol_event_log_root()
```

人话：

```text
AS/TGS 可能多线程写日志，所以创建 Logger、写 Logger map 时要加锁。
```

## 39. protocol_loggers()

代码：

```cpp
std::map<EntityId, std::unique_ptr<Logger>>& protocol_loggers()
```

作用：

```text
保存每个角色对应的 Logger 对象。
```

比如：

```text
AS 一个 Logger
Client1 一个 Logger
TGS 一个 Logger
```

为什么用 `unique_ptr<Logger>`：

```text
Logger 是对象资源，放到 map 里用 unique_ptr 管理生命周期。
```

调用位置：

```text
write_protocol_event()
set_protocol_event_log_root()
```

## 40. protocol_event_log_root()

代码：

```cpp
std::filesystem::path& protocol_event_log_root()
{
    static std::filesystem::path root = default_log_root();
    return root;
}
```

作用：

```text
保存协议事件日志根目录。
```

默认值：

```text
default_log_root()
```

后面可以通过：

```cpp
set_protocol_event_log_root(...)
```

修改。

人话：

```text
所有协议事件日志写到哪里，就是这个 root 决定的。
```

## 41. protocol_timestamp_now()

代码：

```cpp
std::string protocol_timestamp_now()
```

作用：

```text
生成当前时间字符串，格式是 HH:MM:SS.mmm。
```

比如：

```text
13:45:08.123
```

实现：

```text
system_clock 获取当前时间。
duration_cast<milliseconds> 取毫秒部分。
localtime_s 转成本地时间。
ostringstream 拼成字符串。
```

调用位置：

```text
format_protocol_event_message()
```

## 42. protocol_app_message()

代码：

```cpp
std::string protocol_app_message(AppCode code)
{
    return std::string("MSG_APP.") + std::string(to_string(code));
}
```

作用：

```text
把 AppCode 转成 monitor 展示的应用消息名。
```

比如：

```text
AppCode::game_move -> MSG_APP.GAME_MOVE
```

调用位置：

```text
default_message_for_packet()
游戏收发日志手动指定 message 时
```

## 43. set_protocol_event_log_root()

代码：

```cpp
void set_protocol_event_log_root(std::filesystem::path root)
{
    std::lock_guard<std::mutex> lock(logger_mutex());
    protocol_event_log_root() = std::move(root);
    protocol_loggers().clear();
}
```

作用：

```text
设置协议事件日志根目录。
```

为什么要加锁：

```text
日志 root 和 logger map 是全局共享状态，多线程访问要保护。
```

为什么 `protocol_loggers().clear()`：

```text
日志目录变了，旧 Logger 写的是旧路径。
清空后，下次写日志会按新路径重新创建 Logger。
```

调用位置：

```text
role_runtime.cpp 加载 config 后
TankGameClient 构造时
```

## 44. format_protocol_event_message() 四参数版本

代码：

```cpp
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override)
{
    return format_protocol_event_message(direction, packet, message_override, timestamp_override,
                                         {});
}
```

作用：

```text
这是一个简化重载。
如果调用方不提供 payload_view，就转去调用五参数版本，payload_view 传空。
```

人话：

```text
大部分包只需要记录 packet/payload hex。
只有 AS_REP/TGS_REP 这类需要额外展示明文密文时，才传 payload_view。
```

## 45. format_protocol_event_message() 五参数版本

代码：

```cpp
std::string format_protocol_event_message(ProtocolDirection direction, const Packet& packet,
                                          std::string_view message_override,
                                          std::string_view timestamp_override,
                                          const ProtocolPayloadView& payload_view)
```

作用：

```text
把一次 Packet 收发格式化成一条日志正文。
```

生成的字段包括：

```text
ts
direction
endpoint
message
category
msg_type / msg_type_raw
src / src_raw
dst / dst_raw
payload_len / payload_len_raw
reserved / reserved_raw
packet_hex
payload_hex
payload_plain_hex
payload_encrypted_hex
field_count
fieldN_name
fieldN_plain_hex
fieldN_encrypted_hex
```

关键步骤：

```cpp
const PacketHeader header = packet_header(packet);
```

先根据 Packet 得到包头信息。

```cpp
const std::string timestamp =
    timestamp_override.empty() ? protocol_timestamp_now() : std::string(timestamp_override);
```

如果没传时间，就用当前时间。  
如果测试传了固定时间，就用传进来的。

```cpp
const std::string message =
    message_override.empty() ? default_message_for_packet(packet)
                             : std::string(message_override);
```

如果没传 message，就根据 Packet 自动生成。  
如果传了，比如游戏消息指定 `MSG_APP.GAME_MOVE`，就用传入的。

最后把所有内容拼成一行 key=value 日志。

人话：

```text
这个函数就是协议监视器日志的“格式化器”。
它把一个 Packet 变成 monitor 能解析的一整行文字。
```

## 46. parse_protocol_event_line()

代码：

```cpp
ProtocolEvent parse_protocol_event_line(const std::string& line)
```

作用：

```text
把日志文件里的一行文本解析成 ProtocolEvent 结构体。
```

它先解析 Logger 前缀：

```text
[role][thread][PACKET_SEND] ...
```

检查：

```text
必须以 [ 开头
必须有三组 []
第三组必须是 PACKET_SEND 或 PACKET_RECV
```

然后：

```cpp
parse_key_values(line.substr(event_end + 2U))
```

解析后面的 key=value。

再把必填字段放进 `ProtocolEvent`：

```text
role
timestamp
direction
endpoint
message
category
header
packet_hex
payload_hex
payload_plain_hex
payload_encrypted_hex
payload_fields
```

处理 payload fields：

```cpp
if (const auto it = values.find("field_count"); it != values.end())
```

如果日志里有字段数量，就按：

```text
field0_name
field0_plain_hex
field0_encrypted_hex
field1_name
...
```

解析成 `event.payload_fields`。

人话：

```text
monitor 读取日志文件时，靠这个函数把一行文本变回结构化数据。
```

## 47. protocol_event_json()

代码：

```cpp
std::string protocol_event_json(const ProtocolEvent& event, std::uint64_t id)
```

作用：

```text
把 ProtocolEvent 转成 JSON 字符串，发给网页 monitor。
```

输出大概包括：

```json
{
  "type": "protocolEvent",
  "id": 1,
  "timestamp": "...",
  "role": "AS",
  "direction": "SEND",
  "endpoint": "AS->Client1",
  "message": "MSG_AS_REP",
  "category": "kerberos",
  "header": {...},
  "packetHex": "...",
  "payloadHex": "...",
  "payloadFields": [...]
}
```

内部有两个 lambda。

第一个：

```cpp
auto field_json = [](const ProtocolFieldView& field) { ... };
```

作用：

```text
把 header 里的一个字段转成 JSON。
```

第二个：

```cpp
auto payload_field_json = [](const ProtocolPayloadView::Field& field) { ... };
```

作用：

```text
把 payload_fields 里的一个字段转成 JSON。
```

人话：

```text
这个函数是后端日志结构到前端展示数据的最后一步。
```

## 48. write_protocol_event() 三参数版本

代码：

```cpp
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override)
{
    write_protocol_event(direction, packet, message_override, {});
}
```

作用：

```text
简化重载。
不传 ProtocolPayloadView 时，转调用四参数版本。
```

调用场景：

```text
普通 Packet 收发，只记录 packet_hex 和 payload_hex。
```

## 49. write_protocol_event() 四参数版本

代码：

```cpp
void write_protocol_event(ProtocolDirection direction, const Packet& packet,
                          std::string_view message_override,
                          const ProtocolPayloadView& payload_view)
```

作用：

```text
真正把协议事件写入日志文件。
```

流程：

第一步，判断这条日志属于哪个角色：

```cpp
const EntityId role = role_for_direction(direction, packet);
```

第二步，如果角色未知，不写：

```cpp
if (!is_known(role))
{
    return;
}
```

第三步，加锁：

```cpp
std::lock_guard<std::mutex> lock(logger_mutex());
```

第四步，找到或创建 Logger：

```cpp
auto& loggers = protocol_loggers();
auto it = loggers.find(role);
if (it == loggers.end())
{
    it = loggers.emplace(role, std::make_unique<Logger>(event_log_path(role))).first;
}
```

第五步，写日志：

```cpp
it->second->write(entity_label(role), "ProtocolMonitor", event_label(direction),
                  format_protocol_event_message(direction, packet, message_override, {},
                                                payload_view));
```

人话：

```text
这个函数就是协议日志的真正出口。
send_packet_logged() 和 recv_packet_logged() 最后都会走到这里。
```

AS 的例子：

```text
AS 收到 AS_REQ：
  recv_packet_logged()
  -> write_protocol_event(RECV, AS_REQ)
  -> 写入 as_pid.txt

AS 发送 AS_REP：
  send_packet_logged(..., view)
  -> write_protocol_event(SEND, AS_REP, view)
  -> 写入 as_pid.txt
```

## 50. 两个文件怎么一起工作

发送一个 Packet 时：

```text
业务代码创建 Packet
  -> make_packet()
send_packet_logged()
  -> serialize_packet()
  -> socket send
  -> write_protocol_event()
  -> format_protocol_event_message()
  -> Logger 写入 protocol_events/*.txt
```

接收一个 Packet 时：

```text
recv_packet_logged()
  -> socket recv
  -> parse_packet_header()
  -> parse_packet()
  -> write_protocol_event()
  -> Logger 写入 protocol_events/*.txt
```

monitor 展示时：

```text
protocol_monitor 读取日志文件
  -> parse_protocol_event_line()
  -> protocol_event_json()
  -> WebSocket 推给网页
```

## 51. 和 AS 的关系

AS 处理 AS_REQ/AS_REP 时会间接用到这两个文件。

AS 收包：

```text
as_process_connection()
  -> recv_packet_logged(socket)
  -> parse_packet_header()
  -> parse_packet()
  -> write_protocol_event(RECV, request)
```

AS 发包：

```text
as_build_encrypted_packet()
  -> make_packet()
send_packet_logged(socket, response, view)
  -> serialize_packet()
  -> write_protocol_event(SEND, response, view)
```

所以：

```text
packet.cpp 让 AS_REQ/AS_REP 能变成网络包。
protocol_event.cpp 让 AS_REQ/AS_REP 能被 monitor 看见。
```

