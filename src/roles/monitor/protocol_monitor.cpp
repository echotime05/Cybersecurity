#include "cyber/monitor/protocol_monitor.hpp"

#include "cyber/protocol/protocol_event.hpp"
#include "cyber/ui/websocket.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace cyber::monitor
{
namespace
{
struct PendingProtocolEvent
{
    ProtocolEvent event;
    std::filesystem::path file;
    std::uint64_t sequence = 0;
};

std::string dedupe_key(const ProtocolEvent& event)
{
    std::ostringstream oss;
    oss << event.role << '|' << event.direction << '|' << event.endpoint << '|'
        << event.message << '|' << event.header.msg_type.raw << '|' << event.header.src.raw
        << '|' << event.header.dst.raw << '|' << event.header.payload_len.raw << '|'
        << event.header.reserved.raw << '|' << event.payload_hex;
    return oss.str();
}

std::size_t detail_score(const ProtocolEvent& event)
{
    std::size_t score = 0;
    if (!event.payload_plain_hex.empty())
    {
        ++score;
    }
    if (!event.payload_encrypted_hex.empty())
    {
        ++score;
    }
    score += event.payload_fields.size();
    return score;
}
} // namespace

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

    std::vector<PendingProtocolEvent> pending;
    std::map<std::string, std::size_t> pending_by_packet;
    std::uint64_t sequence = 0;
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
                PendingProtocolEvent item{parse_protocol_event_line(line), file, sequence++};
                const std::string key = dedupe_key(item.event);
                const auto existing = pending_by_packet.find(key);
                if (existing == pending_by_packet.end())
                {
                    pending_by_packet[key] = pending.size();
                    pending.push_back(std::move(item));
                }
                else
                {
                    PendingProtocolEvent& stored = pending[existing->second];
                    const bool adjacent_same_file =
                        existing->second + 1U == pending.size() && stored.file == item.file;
                    if (adjacent_same_file && detail_score(item.event) > detail_score(stored.event))
                    {
                        item.event.timestamp = stored.event.timestamp;
                        item.sequence = stored.sequence;
                        stored = std::move(item);
                    }
                    else
                    {
                        pending_by_packet[key] = pending.size();
                        pending.push_back(std::move(item));
                    }
                }
            }
            catch (const std::exception&)
            {
            }
        }
        offset = size;
    }

    std::stable_sort(pending.begin(), pending.end(),
                     [](const PendingProtocolEvent& lhs, const PendingProtocolEvent& rhs) {
                         if (lhs.event.timestamp != rhs.event.timestamp)
                         {
                             return lhs.event.timestamp < rhs.event.timestamp;
                         }
                         if (lhs.file != rhs.file)
                         {
                             return lhs.file < rhs.file;
                         }
                         return lhs.sequence < rhs.sequence;
                     });

    std::vector<std::string> events;
    events.reserve(pending.size());
    for (const PendingProtocolEvent& item : pending)
    {
        events.push_back(protocol_event_json(item.event, next_id_++));
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
