#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cyber/shared/net_socket.hpp"

#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * @file net_socket.cpp
 * @brief 网络套接字生命周期管理模块，封装了 TCP 监听、连接、接收以及底层配置的 Winsock API。
 */

namespace cyber
{
namespace
{
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

// 将 Windows SOCKET 转成项目内 SocketHandle。
SocketHandle to_handle(NativeSocket socket)
{
    return static_cast<SocketHandle>(socket);
}

// 将项目内 SocketHandle 转回 Windows SOCKET。
NativeSocket native_socket(SocketHandle socket)
{
    return static_cast<SOCKET>(socket);
}

// 生成包含 WSAGetLastError 的 socket 错误文本。
std::string last_socket_error(const char* operation)
{
    return std::string(operation) + " failed, WSAGetLastError=" +
           std::to_string(WSAGetLastError());
}

// 根据 TcpEndpoint 构造 IPv4 sockaddr_in。
sockaddr_in make_sockaddr(const TcpEndpoint& endpoint)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.ip.c_str(), &addr.sin_addr) != 1)
    {
        throw std::runtime_error("invalid IPv4 address: " + endpoint.ip);
    }
    return addr;
}

// 如果 socket 有效则关闭它，用于异常清理。
void close_if_valid(NativeSocket socket)
{
    if (socket != kInvalidSocket)
    {
        close_socket(to_handle(socket));
    }
}
} // namespace


/**
 * @brief 创建 TCP 监听 socket，并绑定指定 IP 和端口。（只负责监听，不会用来收发数据）
 * @param endpoint 监听的本地 IP 和端口。
 * @param backlog 操作系统允许处于半连接/全连接队列的最大积压数。
 * @return 创建并监听成功的 SocketHandle。
 */
SocketHandle listen_tcp(const TcpEndpoint& endpoint, int backlog)
{
    NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == kInvalidSocket)
    {
        throw std::runtime_error(last_socket_error("socket"));
    }

    try
    {
        int reuse = 1;

        // SO_REUSEADDR：允许端口复用，使得在服务器重启时能够快速重新绑定同一端口，避免等待 TIME_WAIT 状态结束。
        if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                       sizeof(reuse)) != 0)
        {
            throw std::runtime_error(last_socket_error("setsockopt"));
        }

        const sockaddr_in addr = make_sockaddr(endpoint);
        if (bind(socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            throw std::runtime_error(last_socket_error("bind"));
        }
        if (listen(socket, backlog) != 0)
        {
            throw std::runtime_error(last_socket_error("listen"));
        }
    }
    catch (...)
    {
        close_if_valid(socket);
        throw;
    }

    return to_handle(socket);
}


/**
 * @brief 接受一个 TCP 客户端的连接。
 * @param listen_socket 处于监听状态的 socket。
 * @param peer 输出参数：如果不为空，将写入客户端的 "IP:端口" 字符串。
 * @return 建立好连接的新 SocketHandle，该socket与特定客户端通信。以实现服务器可以同时保持监听状态并处理已连接的客户端。
 */
SocketHandle accept_tcp(SocketHandle listen_socket, std::string* peer)
{
    sockaddr_in addr{};
    int len = sizeof(addr);
    NativeSocket accepted =
        accept(native_socket(listen_socket), reinterpret_cast<sockaddr*>(&addr), &len);
    if (accepted == kInvalidSocket)
    {
        throw std::runtime_error(last_socket_error("accept"));
    }

    try
    {
        set_tcp_nodelay(to_handle(accepted));
    }
    catch (...)
    {
        close_if_valid(accepted);
        throw;
    }

    if (peer != nullptr)
    {
        char host[INET_ADDRSTRLEN] = {};
        const char* converted = inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host));
        if (converted == nullptr)
        {
            *peer = "unknown";
        }
        else
        {
            *peer = std::string(host) + ":" + std::to_string(ntohs(addr.sin_port));
        }
    }

    return to_handle(accepted);
}

// 主动连接指定 TCP 服务端，并设置 TCP_NODELAY。
SocketHandle connect_tcp(const TcpEndpoint& endpoint)
{
    NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == kInvalidSocket)
    {
        throw std::runtime_error(last_socket_error("socket"));
    }

    try
    {
        const sockaddr_in addr = make_sockaddr(endpoint);
        if (connect(socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            throw std::runtime_error(last_socket_error("connect"));
        }
        set_tcp_nodelay(to_handle(socket));
    }
    catch (...)
    {
        close_if_valid(socket);
        throw;
    }

    return to_handle(socket);
}

// 给 socket 开启 TCP_NODELAY，降低小报文延迟。
// 关闭Nagle算法：TCP_NODELAY选项用于禁用Nagle算法，这个算法会将小的网络数据包合并成一个更大的包来发送，以减少网络拥塞，但在某些实时应用中可能会增加延迟。通过设置TCP_NODELAY，可以让每个小数据包立即发送，适合需要低延迟的游戏通信场景。
void set_tcp_nodelay(SocketHandle socket)
{
    int flag = 1;
    if (setsockopt(native_socket(socket), IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0)
    {
        throw std::runtime_error(last_socket_error("setsockopt TCP_NODELAY"));
    }
}
} // namespace cyber
