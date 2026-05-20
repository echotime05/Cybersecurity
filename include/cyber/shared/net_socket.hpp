#pragma once

#include "cyber/shared/net_packet.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
// TCP 监听或连接目标，所有角色启动时都用 ip+port 表示网络端点。
struct TcpEndpoint
{
    std::string ip;
    std::uint16_t port = 0;
};

// 在指定端点创建 TCP 监听 socket。
SocketHandle listen_tcp(const TcpEndpoint& endpoint, int backlog = 16);
// 接受一个 TCP 连接，并可返回对端地址文本。
SocketHandle accept_tcp(SocketHandle listen_socket, std::string* peer = nullptr);
// 主动连接指定 TCP 端点。
SocketHandle connect_tcp(const TcpEndpoint& endpoint);
// 关闭 Nagle 算法，降低游戏小报文交互延迟。
void set_tcp_nodelay(SocketHandle socket);
} // namespace cyber
