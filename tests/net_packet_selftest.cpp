#include "cyber/common/log_parser.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/packet.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

#ifdef _WIN32
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
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;

cyber::SocketHandle to_handle(NativeSocket socket)
{
    return static_cast<cyber::SocketHandle>(socket);
}

void require_socket(NativeSocket socket, const char* message)
{
    if (socket < 0)
    {
        throw std::runtime_error(message);
    }
}

void require_socket_result(int result, const char* message)
{
    if (result < 0)
    {
        throw std::runtime_error(message);
    }
}
#endif

std::vector<std::string> read_lines(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("failed to read log file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }
    return lines;
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
} // namespace

int main()
{
    try
    {
        cyber::SocketRuntime runtime;

        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("cyber_net_packet_selftest_" + std::to_string(static_cast<long long>(now)));
        const std::filesystem::path server_log_path = dir / "server.log";
        const std::filesystem::path client_log_path = dir / "client.log";

        NativeSocket listen_socket = make_loopback_listener();
        const std::uint16_t port = bound_port(listen_socket);

        std::exception_ptr server_error;
        std::thread server([&]() {
            NativeSocket accepted = kInvalidSocket;
            try
            {
                accepted = accept(listen_socket, nullptr, nullptr);
                require_socket(accepted, "accept failed");

                cyber::Logger logger(server_log_path);
                const cyber::Packet received =
                    cyber::recv_packet_logged(to_handle(accepted), logger, "V", "VWorker-C1-GAME");
                require(received.msg_type == cyber::MsgType::app, "server msg_type mismatch");
                require(cyber::parse_app_code(received.payload) == cyber::AppCode::key_down,
                        "server app_code mismatch");

                const cyber::Packet ack = cyber::make_packet(
                    cyber::MsgType::app, cyber::EntityId::v, cyber::EntityId::client1,
                    cyber::make_app_payload(cyber::AppCode::app_ack, cyber::Bytes{0x01}));
                cyber::send_packet_logged(to_handle(accepted), ack, logger, "V",
                                           "VWorker-C1-GAME");
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
            cyber::Logger client_logger(client_log_path);
            const cyber::Packet key_down = cyber::make_packet(
                cyber::MsgType::app, cyber::EntityId::client1, cyber::EntityId::v,
                cyber::make_app_payload(cyber::AppCode::key_down,
                                        cyber::Bytes{static_cast<std::uint8_t>('W')}));
            cyber::send_packet_logged(to_handle(client_socket), key_down, client_logger, "Client",
                                       "SendThread");
            const cyber::Packet ack = cyber::recv_packet_logged(to_handle(client_socket),
                                                                client_logger, "Client", "RxThread");
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

        const std::vector<std::string> server_lines = read_lines(server_log_path);
        const std::vector<std::string> client_lines = read_lines(client_log_path);
        require(server_lines.size() == 2U, "unexpected server log line count");
        require(client_lines.size() == 2U, "unexpected client log line count");

        cyber::LogEntry entry;
        require(cyber::parse_log_line(server_lines[0], entry), "server recv log did not parse");
        require(entry.entity == "V", "server recv entity mismatch");
        require(entry.event == "PACKET_RECV", "server recv event mismatch");
        require(entry.message.find("msg_type=MSG_APP") != std::string::npos,
                "server recv msg_type missing");
        require(entry.message.find("APP_code=KEY_DOWN") != std::string::npos,
                "server recv app code missing");

        require(cyber::parse_log_line(client_lines[1], entry), "client recv log did not parse");
        require(entry.entity == "Client", "client recv entity mismatch");
        require(entry.event == "PACKET_RECV", "client recv event mismatch");
        require(entry.message.find("APP_code=APP_ACK") != std::string::npos,
                "client ack app code missing");

        std::filesystem::remove_all(dir);
        std::cout << "net_packet_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "net_packet_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
