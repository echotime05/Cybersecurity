#include "cyber/monitor/protocol_monitor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "protocol_monitor_tail_selftest";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);

        const std::filesystem::path file = dir / "client_01_1234.txt";
        {
            std::ofstream out(file);
            out << "[Client1][ProtocolMonitor][PACKET_SEND] "
                << "ts=12:03:15.044 direction=SEND endpoint=Client1->V "
                << "message=MSG_APP.GAME_MOVE category=app msg_type=MSG_APP "
                << "msg_type_raw=0x66 src=Client1 src_raw=0x01 dst=V dst_raw=0x13 "
                << "payload_len=2 payload_len_raw=0x00000002 reserved=0 "
                << "reserved_raw=0x00000000 payload_hex=aabb\n";
        }

        cyber::monitor::ProtocolEventTailer tailer(dir);
        const auto first = tailer.poll_json_events();
        require(first.size() == 1U, "expected first event");
        require(first[0].find("\"type\":\"protocolEvent\"") != std::string::npos,
                "first event JSON type missing");
        require(first[0].find("\"message\":\"MSG_APP.GAME_MOVE\"") != std::string::npos,
                "first event message missing");

        {
            std::ofstream out(file, std::ios::app);
            out << "[V][ProtocolMonitor][PACKET_RECV] "
                << "ts=12:03:15.061 direction=RECV endpoint=Client1->V "
                << "message=MSG_APP.GAME_MOVE category=app msg_type=MSG_APP "
                << "msg_type_raw=0x66 src=Client1 src_raw=0x01 dst=V dst_raw=0x13 "
                << "payload_len=2 payload_len_raw=0x00000002 reserved=0 "
                << "reserved_raw=0x00000000 payload_hex=aabb\n";
        }

        const auto second = tailer.poll_json_events();
        require(second.size() == 1U, "expected appended event only");
        require(second[0].find("\"role\":\"V\"") != std::string::npos,
                "second event role missing");
        require(second[0].find("\"direction\":\"RECV\"") != std::string::npos,
                "second event direction missing");

        std::filesystem::remove_all(dir);
        std::cout << "protocol_monitor_tail_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "protocol_monitor_tail_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
