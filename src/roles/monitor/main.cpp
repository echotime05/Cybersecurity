#include "cyber/shared/net_socket.hpp"
#include "cyber/shared/runtime_paths.hpp"
#include "cyber/monitor/protocol_monitor.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
// 打印 Monitor 可执行文件支持的命令行参数。
void print_usage()
{
    std::cout << "usage: monitor.exe [--ui-port PORT] [--events-dir DIR]\n";
}
} // namespace

// Monitor 可执行文件入口，启动协议事件 WebSocket 推送服务。
int main(int argc, char** argv)
{
    try
    {
        std::uint16_t port = 7010;
        std::filesystem::path events_dir = cyber::protocol_events_dir();

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                print_usage();
                return 0;
            }
            if (arg == "--ui-port" && i + 1 < argc)
            {
                port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
            }
            else if (arg == "--events-dir" && i + 1 < argc)
            {
                events_dir = argv[++i];
            }
            else
            {
                throw std::runtime_error("unknown monitor argument: " + arg);
            }
        }

        cyber::SocketRuntime runtime;
        cyber::monitor::ProtocolMonitorServer server({"127.0.0.1", port}, events_dir);
        server.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "monitor failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
