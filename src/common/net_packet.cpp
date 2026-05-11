#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cyber/common/net_packet.hpp"

#include "cyber/common/crypto.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cyber
{
namespace
{
#ifdef _WIN32
SOCKET native_socket(SocketHandle socket)
{
    return static_cast<SOCKET>(socket);
}

std::string socket_error_message(const char* operation)
{
    return std::string(operation) + " failed, WSAGetLastError=" +
           std::to_string(WSAGetLastError());
}
#else
int native_socket(SocketHandle socket)
{
    return static_cast<int>(socket);
}

std::string socket_error_message(const char* operation)
{
    return std::string(operation) + " failed, errno=" + std::to_string(errno) + " (" +
           std::strerror(errno) + ")";
}
#endif

void send_all(SocketHandle socket, const Bytes& bytes)
{
    std::size_t sent = 0;
    while (sent < bytes.size())
    {
        const std::size_t remaining = bytes.size() - sent;
        const int chunk = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
#ifdef _WIN32
        const int n =
            ::send(native_socket(socket), reinterpret_cast<const char*>(bytes.data() + sent), chunk, 0);
#else
        const int n = static_cast<int>(
            ::send(native_socket(socket), bytes.data() + sent, static_cast<std::size_t>(chunk), 0));
#endif
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
#ifdef _WIN32
        const int n =
            ::recv(native_socket(socket), reinterpret_cast<char*>(out + received), chunk, 0);
#else
        const int n = static_cast<int>(
            ::recv(native_socket(socket), out + received, static_cast<std::size_t>(chunk), 0));
#endif
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

std::string hex_id(EntityId id)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
        << static_cast<int>(static_cast<std::uint8_t>(id));
    return oss.str();
}

std::string format_payload_summary(const Packet& packet)
{
    const auto encrypted_summary = [&](const char* label) {
        std::ostringstream oss;
        oss << label << "{encrypted=true, cipher_hash=0x" << std::hex << std::uppercase
            << std::setfill('0') << std::setw(16) << hash64(packet.payload) << std::dec
            << ", cipher_len=" << packet.payload.size() << '}';
        return oss.str();
    };

    if (packet.msg_type == MsgType::as_rep || packet.msg_type == MsgType::tgs_rep ||
        packet.msg_type == MsgType::v_auth_rep || packet.msg_type == MsgType::cert_c2v ||
        packet.msg_type == MsgType::cert_v2c)
    {
        return encrypted_summary("payload");
    }

    if (packet.msg_type == MsgType::app && !packet.payload.empty() &&
        packet.payload.size() % 8U == 0U)
    {
        return encrypted_summary("MSG_APP");
    }

    if (packet.msg_type == MsgType::app && !packet.payload.empty())
    {
        const AppCode code = parse_app_code(packet.payload);
        std::ostringstream oss;
        oss << "MSG_APP{APP_code=" << to_string(code)
            << ", app_payload_len=" << parse_app_payload(packet.payload).size() << '}';
        return oss.str();
    }

    if (packet.msg_type == MsgType::error && !packet.payload.empty())
    {
        const ErrorCode code = parse_error_code(packet.payload);
        std::ostringstream oss;
        oss << "MSG_ERROR{err_code=" << to_string(code)
            << ", err_msg=" << parse_error_message(packet.payload) << '}';
        return oss.str();
    }

    std::ostringstream oss;
    oss << "payload{len=" << packet.payload.size();
    if (!packet.payload.empty() && packet.payload.size() <= 32U)
    {
        oss << ", hex=" << bytes_to_hex(packet.payload);
    }
    oss << '}';
    return oss.str();
}
} // namespace

SocketRuntime::SocketRuntime()
{
#ifdef _WIN32
    WSADATA data;
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed, result=" + std::to_string(result));
    }
#endif
}

SocketRuntime::~SocketRuntime()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

void close_socket(SocketHandle socket)
{
#ifdef _WIN32
    if (native_socket(socket) != INVALID_SOCKET)
    {
        closesocket(native_socket(socket));
    }
#else
    if (native_socket(socket) >= 0)
    {
        close(native_socket(socket));
    }
#endif
}

bool send_packet_logged(SocketHandle socket, const Packet& packet, Logger& logger,
                        std::string_view entity, std::string_view thread_name)
{
    send_all(socket, serialize_packet(packet));
    logger.write(entity, thread_name, "PACKET_SEND", format_packet_log_message(packet));
    return true;
}

Packet recv_packet_logged(SocketHandle socket, Logger& logger, std::string_view entity,
                          std::string_view thread_name)
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
    logger.write(entity, thread_name, "PACKET_RECV", format_packet_log_message(packet));
    return packet;
}

std::string format_packet_log_message(const Packet& packet)
{
    const PacketHeader header = packet_header(packet);
    std::ostringstream oss;
    oss << "header={msg_type=" << to_string(header.msg_type) << ", src_ID="
        << hex_id(header.src) << ", dst_ID=" << hex_id(header.dst)
        << ", payload_len=" << header.payload_len << ", reserved=" << header.reserved
        << "}; payload=" << format_payload_summary(packet);
    return oss.str();
}
} // namespace cyber
