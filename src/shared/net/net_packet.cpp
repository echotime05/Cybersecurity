#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cyber/shared/net_packet.hpp"

#include "cyber/protocol/protocol_event.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

/**
 * 核心职责：在TCP字节流之上实现基于固定头格式的报文收发，收发同时
 * 自动写协议事件日志
 */

namespace cyber
{
namespace
{
// 将项目内 SocketHandle 转成 Windows SOCKET。
SOCKET native_socket(SocketHandle socket)
{
    return static_cast<SOCKET>(socket);
}

// 生成包含 WSAGetLastError 的网络错误文本。
std::string socket_error_message(const char* operation)
{
    return std::string(operation) + " failed, WSAGetLastError=" +
           std::to_string(WSAGetLastError());
}

// 循环发送直到全部字节写入 socket。
void send_all(SocketHandle socket, const Bytes& bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size())
    {
        const std::size_t remaining = bytes.size() - sent;
        const int chunk = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int n =
            ::send(native_socket(socket), reinterpret_cast<const char*>(bytes.data() + sent), chunk, 0);
        if (n <= 0)
        {
            throw PacketError(socket_error_message("send"));
        }
        sent += static_cast<std::size_t>(n);
    }
}

// 循环接收指定长度的字节，连接关闭或出错时抛异常。
void recv_exact(SocketHandle socket, std::uint8_t* out, std::size_t size)
{
    std::size_t received = 0;
    while (received < size)
    {
        const std::size_t remaining = size - received;
        const int chunk = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int n =
            ::recv(native_socket(socket), reinterpret_cast<char*>(out + received), chunk, 0);
        if (n == 0)
        {
            throw PacketError("recv failed, peer closed connection");
        }
        if (n < 0)
        {
            throw PacketError(socket_error_message("recv"));
        }
        received += static_cast<std::size_t>(n);
    }
}

} // namespace

// 初始化 Windows socket 运行时。
SocketRuntime::SocketRuntime()
{
    WSADATA data;
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed, result=" + std::to_string(result));
    }
}

// 清理 Windows socket 运行时。
SocketRuntime::~SocketRuntime()
{
    WSACleanup();
}

// 关闭 socket 句柄。
void close_socket(SocketHandle socket)
{
    if (native_socket(socket) != INVALID_SOCKET)
    {
        closesocket(native_socket(socket));
    }
}

// 发送报文并记录协议事件，payload 使用默认视图。
bool send_packet_logged(SocketHandle socket, const Packet& packet)
{
    return send_packet_logged(socket, packet, {});
}

// 发送报文并记录协议事件，调用方可传入 payload 明文/密文视图。
bool send_packet_logged(SocketHandle socket, const Packet& packet,
                        const ProtocolPayloadView& payload_view)
{
    send_all(socket, serialize_packet(packet));
    if (packet.msg_type != MsgType::app)
    {
        try
        {
            write_protocol_event(ProtocolDirection::send, packet, {}, payload_view);
        }
        catch (const std::exception&)
        {
        }
    }
    return true;
}

// 接收完整报文并记录协议事件。
Packet recv_packet_logged(SocketHandle socket)
{
    // 第一阶段：先接收固定长度的报文头，解析出 payload 长度，再接收剩余的 payload 字节，最后将完整报文解析成 Packet 结构体。
    Bytes raw(kPacketHeaderSize);
    recv_exact(socket, raw.data(), raw.size());

    // 反序列化包头，从中获取payload的长度信息
    const PacketHeader header = parse_packet_header(raw);

    // 安全防御额：防止恶意发包者发送一个非常大的 payload_len 来耗尽服务器资源，因此在这里设置一个合理的最大值限制，如果超过则抛出异常。
    if (header.payload_len > kMaxPacketPayloadSize)
    {
        throw PacketError("packet payload exceeds max allowed size");
    }

    // 第二阶段：接收并解析payload
    // 扩展缓冲区以接纳完整报文，先接收剩余的 payload 字节，再将整个报文解析成 Packet 结构体。
    raw.resize(kPacketHeaderSize + header.payload_len);

    if (header.payload_len > 0)
    {
        recv_exact(socket, raw.data() + kPacketHeaderSize, header.payload_len);
    }

    // 将完整的字节流解析为packet结构体
    Packet packet = parse_packet(raw);

    //第三阶段：协议事件记录
    if (packet.msg_type != MsgType::app)
    {
        try
        {
            write_protocol_event(ProtocolDirection::recv, packet);
        }
        catch (const std::exception&)
        {
        }
    }
    return packet;
}

} // namespace cyber
