#include "cyber/monitor/protocol_monitor.hpp"

#include "cyber/common/protocol_event.hpp"
#include "cyber/ui/websocket.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace cyber::monitor
{
ProtocolEventTailer::ProtocolEventTailer(std::filesystem::path events_dir)
    : events_dir_(std::move(events_dir))
{
}

std::vector<std::string> ProtocolEventTailer::poll_json_events()
{
    std::vector<std::filesystem::path> files;
    if (std::filesystem::exists(events_dir_))
    {
        for (const auto& entry : std::filesystem::directory_iterator(events_dir_))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".txt")
            {
                files.push_back(entry.path());
            }
        }
    }
    std::sort(files.begin(), files.end());

    std::vector<std::string> events;
    for (const std::filesystem::path& file : files)
    {
        const std::uintmax_t size = std::filesystem::file_size(file);
        std::uintmax_t& offset = offsets_[file];
        if (size < offset)
        {
            offset = 0;
        }

        std::ifstream in(file);
        if (!in)
        {
            continue;
        }
        in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }
            try
            {
                events.push_back(protocol_event_json(parse_protocol_event_line(line), next_id_++));
            }
            catch (const std::exception&)
            {
            }
        }
        offset = size;
    }
    return events;
}

ProtocolMonitorServer::ProtocolMonitorServer(TcpEndpoint endpoint, std::filesystem::path events_dir)
    : endpoint_(std::move(endpoint)), events_dir_(std::move(events_dir))
{
}

ProtocolMonitorServer::~ProtocolMonitorServer()
{
    stop();
}

void ProtocolMonitorServer::run()
{
    listener_ = listen_tcp(endpoint_);
    std::cout << "Protocol monitor listening on " << endpoint_.ip << ':' << endpoint_.port
              << " events=" << events_dir_.string() << '\n';
    run_until_stopped();
}

std::uint16_t ProtocolMonitorServer::start_for_test()
{
    listener_ = listen_tcp(endpoint_);
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getsockname(static_cast<SOCKET>(listener_), reinterpret_cast<sockaddr*>(&addr), &len) != 0)
    {
        throw std::runtime_error("getsockname failed for protocol monitor");
    }
    endpoint_.port = ntohs(addr.sin_port);
    return endpoint_.port;
}

void ProtocolMonitorServer::run_until_stopped()
{
    accept_loop();
}

void ProtocolMonitorServer::stop()
{
    stopping_ = true;
    if (listener_ != 0)
    {
        close_socket(listener_);
        listener_ = 0;
    }
}

void ProtocolMonitorServer::accept_loop()
{
    while (!stopping_)
    {
        try
        {
            SocketHandle accepted = accept_tcp(listener_);
            std::thread(&ProtocolMonitorServer::handle_client, this, accepted).detach();
        }
        catch (const std::exception&)
        {
            if (stopping_)
            {
                return;
            }
            throw;
        }
    }
}

void ProtocolMonitorServer::handle_client(SocketHandle socket)
{
    ProtocolEventTailer tailer(events_dir_);
    try
    {
        ui::perform_websocket_server_handshake(socket);
        while (!stopping_)
        {
            for (const std::string& event : tailer.poll_json_events())
            {
                ui::send_ws_text(socket, event);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }
    catch (const std::exception&)
    {
    }
    close_socket(socket);
}
} // namespace cyber::monitor
