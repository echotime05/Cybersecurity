#include "cyber/shared/net_packet.hpp"
#include "cyber/shared/net_socket.hpp"
#include "cyber/protocol/packet.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

cyber::SocketHandle to_handle(NativeSocket socket)
{
    return static_cast<cyber::SocketHandle>(socket);
}

void require_socket(NativeSocket socket, const char* message)
{
    if (socket == INVALID_SOCKET)
    {
        throw std::runtime_error(std::string(message) + ", WSAGetLastError=" +
                                 std::to_string(WSAGetLastError()));
    }
}

void require_socket_result(int result, const char* message)
{
    if (result == SOCKET_ERROR)
    {
        throw std::runtime_error(std::string(message) + ", WSAGetLastError=" +
                                 std::to_string(WSAGetLastError()));
    }
}

void require_tcp_nodelay_enabled(cyber::SocketHandle socket, const char* message)
{
    int value = 0;
    int len = sizeof(value);
    require_socket_result(
        getsockopt(static_cast<SOCKET>(socket), IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<char*>(&value), &len),
        "getsockopt TCP_NODELAY failed");
    require(value != 0, message);
}

std::uint16_t bound_port(NativeSocket listen_socket)
{
    sockaddr_in addr{};
    int len = sizeof(addr);
    require_socket_result(
        getsockname(listen_socket, reinterpret_cast<sockaddr*>(&addr), &len),
        "getsockname failed");
    return ntohs(addr.sin_port);
}

NativeSocket make_loopback_listener()
{
    NativeSocket listen_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    require_socket(listen_socket, "socket failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    require_socket_result(
        bind(listen_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)),
        "bind failed");
    require_socket_result(listen(listen_socket, 1), "listen failed");
    return listen_socket;
}

NativeSocket connect_loopback(std::uint16_t port)
{
    NativeSocket client_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    require_socket(client_socket, "client socket failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    require_socket_result(
        connect(client_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)),
        "connect failed");
    return client_socket;
}

void verify_helper_sockets_enable_tcp_nodelay()
{
    cyber::SocketHandle listener = cyber::listen_tcp({"127.0.0.1", 0}, 1);
    const std::uint16_t port = bound_port(static_cast<SOCKET>(listener));

    std::exception_ptr server_error;
    std::thread server([&]() {
        cyber::SocketHandle accepted = 0;
        try
        {
            accepted = cyber::accept_tcp(listener);
            require_tcp_nodelay_enabled(accepted, "accept_tcp did not enable TCP_NODELAY");
        }
        catch (...)
        {
            server_error = std::current_exception();
        }
        if (accepted != 0)
        {
            cyber::close_socket(accepted);
        }
    });

    cyber::SocketHandle client = 0;
    try
    {
        client = cyber::connect_tcp({"127.0.0.1", port});
        require_tcp_nodelay_enabled(client, "connect_tcp did not enable TCP_NODELAY");
    }
    catch (...)
    {
        if (client != 0)
        {
            cyber::close_socket(client);
        }
        cyber::close_socket(listener);
        server.join();
        throw;
    }

    server.join();
    cyber::close_socket(client);
    cyber::close_socket(listener);
    if (server_error)
    {
        std::rethrow_exception(server_error);
    }
}
} // namespace

int main()
{
    try
    {
        cyber::SocketRuntime runtime;
        verify_helper_sockets_enable_tcp_nodelay();

        NativeSocket listen_socket = make_loopback_listener();
        const std::uint16_t port = bound_port(listen_socket);

        std::exception_ptr server_error;
        std::thread server([&]() {
            NativeSocket accepted = kInvalidSocket;
            try
            {
                accepted = accept(listen_socket, nullptr, nullptr);
                require_socket(accepted, "accept failed");

                const cyber::Packet received =
                    cyber::recv_packet_logged(to_handle(accepted));
                require(received.msg_type == cyber::MsgType::app, "server msg_type mismatch");
                require(cyber::parse_app_code(received.payload) == cyber::AppCode::game_move,
                        "server app_code mismatch");

                const cyber::Packet ack = cyber::make_packet(
                    cyber::MsgType::app, cyber::EntityId::v, cyber::EntityId::client1,
                    cyber::make_app_payload(cyber::AppCode::app_ack, cyber::Bytes{0x01}));
                cyber::send_packet_logged(to_handle(accepted), ack);
            }
            catch (...)
            {
                server_error = std::current_exception();
            }
            if (accepted != kInvalidSocket)
            {
                cyber::close_socket(to_handle(accepted));
            }
        });

        NativeSocket client_socket = connect_loopback(port);
        {
            const cyber::Packet move = cyber::make_packet(
                cyber::MsgType::app, cyber::EntityId::client1, cyber::EntityId::v,
                cyber::make_app_payload(cyber::AppCode::game_move,
                                        cyber::Bytes{static_cast<std::uint8_t>('W')}));
            cyber::send_packet_logged(to_handle(client_socket), move);
            const cyber::Packet ack = cyber::recv_packet_logged(to_handle(client_socket));
            require(ack.msg_type == cyber::MsgType::app, "client ack msg_type mismatch");
            require(cyber::parse_app_code(ack.payload) == cyber::AppCode::app_ack,
                    "client ack app_code mismatch");
        }
        cyber::close_socket(to_handle(client_socket));
        cyber::close_socket(to_handle(listen_socket));
        server.join();
        if (server_error)
        {
            std::rethrow_exception(server_error);
        }

        std::cout << "net_packet_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "net_packet_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
