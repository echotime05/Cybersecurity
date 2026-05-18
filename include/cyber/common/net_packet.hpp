#pragma once

#include "cyber/common/packet.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace cyber
{
using SocketHandle = std::uintptr_t;
struct ProtocolPayloadView;

constexpr std::uint32_t kMaxPacketPayloadSize = 16U * 1024U * 1024U;

class SocketRuntime
{
public:
    SocketRuntime();
    ~SocketRuntime();

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;
};

void close_socket(SocketHandle socket);

bool send_packet_logged(SocketHandle socket, const Packet& packet);
bool send_packet_logged(SocketHandle socket, const Packet& packet,
                        const ProtocolPayloadView& payload_view);
Packet recv_packet_logged(SocketHandle socket);

} // namespace cyber
