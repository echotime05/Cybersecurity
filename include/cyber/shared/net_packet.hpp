#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace cyber
{
// 跨平台 socket 句柄的项目内表示，Windows 下实际保存 SOCKET 值。
using SocketHandle = std::uintptr_t;
struct ProtocolPayloadView;

// 单个报文 payload 的最大允许长度，防止异常长度拖垮进程内存。
constexpr std::uint32_t kMaxPacketPayloadSize = 16U * 1024U * 1024U;

// 网络运行时初始化器，在 Windows 下负责 WSAStartup/WSACleanup。
class SocketRuntime
{
public:
    // 初始化当前进程的 socket 运行时。
    SocketRuntime();
    // 清理当前进程的 socket 运行时。
    ~SocketRuntime();

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
};

// 关闭 socket 句柄，屏蔽不同平台的关闭函数差异。
void close_socket(SocketHandle socket);

// 发送报文并写入协议可视化日志，payload 按原始报文展示。
bool send_packet_logged(SocketHandle socket, const Packet& packet);
// 发送报文并写入协议可视化日志，使用调用方提供的 payload 结构化视图。
bool send_packet_logged(SocketHandle socket, const Packet& packet,
                        const ProtocolPayloadView& payload_view);
// 接收一个完整报文并写入协议可视化日志。
Packet recv_packet_logged(SocketHandle socket);

} // namespace cyber
