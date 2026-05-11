#pragma once

#include "cyber/common/logger.hpp"
#include "cyber/common/packet.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace cyber
{
using SocketHandle = std::uintptr_t;

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

bool send_packet_logged(SocketHandle socket, const Packet& packet, Logger& logger,
                        std::string_view entity, std::string_view thread_name);
Packet recv_packet_logged(SocketHandle socket, Logger& logger, std::string_view entity,
                          std::string_view thread_name);

std::string format_packet_log_message(const Packet& packet);
} // namespace cyber
