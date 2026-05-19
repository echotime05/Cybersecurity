#pragma once

#include "cyber/shared/net_packet.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
struct TcpEndpoint
{
    std::string ip;
    std::uint16_t port = 0;
};

SocketHandle listen_tcp(const TcpEndpoint& endpoint, int backlog = 16);
SocketHandle accept_tcp(SocketHandle listen_socket, std::string* peer = nullptr);
SocketHandle connect_tcp(const TcpEndpoint& endpoint);
void set_tcp_nodelay(SocketHandle socket);
} // namespace cyber
