#pragma once

#include "cyber/shared/net_socket.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cyber::monitor
{
class ProtocolEventTailer
{
public:
    explicit ProtocolEventTailer(std::filesystem::path events_dir);

    std::vector<std::string> poll_json_events();

private:
    std::filesystem::path events_dir_;
    std::map<std::filesystem::path, std::uintmax_t> offsets_;
    std::uint64_t next_id_ = 1;
};

class ProtocolMonitorServer
{
public:
    ProtocolMonitorServer(TcpEndpoint endpoint, std::filesystem::path events_dir);
    ~ProtocolMonitorServer();

    void run();
    std::uint16_t start_for_test();
    void run_until_stopped();
    void stop();

private:
    void accept_loop();
    void handle_client(SocketHandle socket);

    TcpEndpoint endpoint_;
    std::filesystem::path events_dir_;
    SocketHandle listener_ = 0;
    bool stopping_ = false;
};
} // namespace cyber::monitor
