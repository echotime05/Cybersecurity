#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cyber/common/net_socket.hpp"

#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

namespace cyber
{
namespace
{
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

SocketHandle to_handle(NativeSocket socket)
{
    return static_cast<SocketHandle>(socket);
}

NativeSocket native_socket(SocketHandle socket)
{
    return static_cast<SOCKET>(socket);
}

std::string last_socket_error(const char* operation)
{
    return std::string(operation) + " failed, WSAGetLastError=" +
           std::to_string(WSAGetLastError());
}

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

void close_if_valid(NativeSocket socket)
{
    if (socket != kInvalidSocket)
    {
        close_socket(to_handle(socket));
    }
}
} // namespace

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
    }
    catch (...)
    {
        close_if_valid(socket);
        throw;
    }

    return to_handle(socket);
}
} // namespace cyber
