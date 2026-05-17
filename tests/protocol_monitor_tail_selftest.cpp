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

        const std::filesystem::path ordered_dir =
            std::filesystem::temp_directory_path() / "protocol_monitor_tail_order_selftest";
        std::filesystem::remove_all(ordered_dir);
        std::filesystem::create_directories(ordered_dir);
        {
            std::ofstream as_out(ordered_dir / "as_1234.txt");
            as_out << "[AS][ProtocolMonitor][PACKET_SEND] "
                   << "ts=12:03:15.200 direction=SEND endpoint=AS->Client1 "
                   << "message=MSG_AS_REP category=kerberos msg_type=MSG_AS_REP "
                   << "msg_type_raw=0x02 src=AS src_raw=0x11 dst=Client1 dst_raw=0x01 "
                   << "payload_len=1 payload_len_raw=0x00000001 reserved=0 "
                   << "reserved_raw=0x00000000 payload_hex=aa\n";

            std::ofstream client_out(ordered_dir / "client_01_1234.txt");
            client_out << "[Client1][ProtocolMonitor][PACKET_SEND] "
                       << "ts=12:03:15.100 direction=SEND endpoint=Client1->AS "
                       << "message=MSG_AS_REQ category=kerberos msg_type=MSG_AS_REQ "
                       << "msg_type_raw=0x01 src=Client1 src_raw=0x01 dst=AS dst_raw=0x11 "
                       << "payload_len=1 payload_len_raw=0x00000001 reserved=0 "
                       << "reserved_raw=0x00000000 payload_hex=bb\n";
        }

        cyber::monitor::ProtocolEventTailer ordered_tailer(ordered_dir);
        const auto ordered = ordered_tailer.poll_json_events();
        require(ordered.size() == 2U, "expected two ordered events");
        require(ordered[0].find("\"message\":\"MSG_AS_REQ\"") != std::string::npos,
                "events should be ordered by timestamp across files");
        require(ordered[1].find("\"message\":\"MSG_AS_REP\"") != std::string::npos,
                "second ordered event mismatch");

        std::filesystem::remove_all(ordered_dir);

        const std::filesystem::path duplicate_dir =
            std::filesystem::temp_directory_path() / "protocol_monitor_tail_duplicate_selftest";
        std::filesystem::remove_all(duplicate_dir);
        std::filesystem::create_directories(duplicate_dir);
        {
            std::ofstream client_out(duplicate_dir / "client_01_1234.txt");
            client_out << "[Client1][ProtocolMonitor][PACKET_RECV] "
                       << "ts=12:03:15.200 direction=RECV endpoint=AS->Client1 "
                       << "message=MSG_AS_REP category=kerberos msg_type=MSG_AS_REP "
                       << "msg_type_raw=0x02 src=AS src_raw=0x11 dst=Client1 dst_raw=0x01 "
                       << "payload_len=1 payload_len_raw=0x00000001 reserved=0 "
                       << "reserved_raw=0x00000000 payload_hex=aa\n";
            client_out << "[Client1][ProtocolMonitor][PACKET_RECV] "
                       << "ts=12:03:15.210 direction=RECV endpoint=AS->Client1 "
                       << "message=MSG_AS_REP category=kerberos msg_type=MSG_AS_REP "
                       << "msg_type_raw=0x02 src=AS src_raw=0x11 dst=Client1 dst_raw=0x01 "
                       << "payload_len=1 payload_len_raw=0x00000001 reserved=0 "
                       << "reserved_raw=0x00000000 payload_hex=aa payload_plain_hex=bb "
                       << "payload_encrypted_hex=aa\n";
        }

        cyber::monitor::ProtocolEventTailer duplicate_tailer(duplicate_dir);
        const auto deduped = duplicate_tailer.poll_json_events();
        require(deduped.size() == 1U, "expected duplicate protocol events to be merged");
        require(deduped[0].find("\"payloadPlainHex\":\"bb\"") != std::string::npos,
                "dedupe should keep enriched payload view");

        std::filesystem::remove_all(duplicate_dir);

        const std::filesystem::path repeated_dir =
            std::filesystem::temp_directory_path() / "protocol_monitor_tail_repeated_selftest";
        std::filesystem::remove_all(repeated_dir);
        std::filesystem::create_directories(repeated_dir);
        {
            std::ofstream client_out(repeated_dir / "client_01_1234.txt");
            client_out << "[Client1][ProtocolMonitor][PACKET_SEND] "
                       << "ts=12:03:15.300 direction=SEND endpoint=Client1->V "
                       << "message=MSG_APP.GAME_MOVE category=app msg_type=MSG_APP "
                       << "msg_type_raw=0x66 src=Client1 src_raw=0x01 dst=V dst_raw=0x13 "
                       << "payload_len=3 payload_len_raw=0x00000003 reserved=0 "
                       << "reserved_raw=0x00000000 payload_hex=020100\n";
            client_out << "[Client1][ProtocolMonitor][PACKET_SEND] "
                       << "ts=12:03:15.350 direction=SEND endpoint=Client1->V "
                       << "message=MSG_APP.GAME_MOVE category=app msg_type=MSG_APP "
                       << "msg_type_raw=0x66 src=Client1 src_raw=0x01 dst=V dst_raw=0x13 "
                       << "payload_len=3 payload_len_raw=0x00000003 reserved=0 "
                       << "reserved_raw=0x00000000 payload_hex=020100\n";
        }

        cyber::monitor::ProtocolEventTailer repeated_tailer(repeated_dir);
        const auto repeated = repeated_tailer.poll_json_events();
        require(repeated.size() == 2U, "actual repeated identical packets should be preserved");

        std::filesystem::remove_all(repeated_dir);
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
