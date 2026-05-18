#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cyber/common/net_packet.hpp"

#include "cyber/common/protocol_event.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace cyber
{
namespace
{
SOCKET native_socket(SocketHandle socket)
{
    return static_cast<SOCKET>(socket);
}

std::string socket_error_message(const char* operation)
{
    return std::string(operation) + " failed, WSAGetLastError=" +
           std::to_string(WSAGetLastError());
}

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

SocketRuntime::SocketRuntime()
{
    WSADATA data;
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed, result=" + std::to_string(result));
    }
}

SocketRuntime::~SocketRuntime()
{
    WSACleanup();
}

void close_socket(SocketHandle socket)
{
    if (native_socket(socket) != INVALID_SOCKET)
    {
        closesocket(native_socket(socket));
    }
}

bool send_packet_logged(SocketHandle socket, const Packet& packet)
{
    return send_packet_logged(socket, packet, {});
}

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

Packet recv_packet_logged(SocketHandle socket)
{
    Bytes raw(kPacketHeaderSize);
    recv_exact(socket, raw.data(), raw.size());

    const PacketHeader header = parse_packet_header(raw);
    if (header.payload_len > kMaxPacketPayloadSize)
    {
        throw PacketError("packet payload exceeds max allowed size");
    }

    raw.resize(kPacketHeaderSize + header.payload_len);
    if (header.payload_len > 0)
    {
        recv_exact(socket, raw.data() + kPacketHeaderSize, header.payload_len);
    }

    Packet packet = parse_packet(raw);
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
