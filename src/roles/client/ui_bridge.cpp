#include "cyber/ui/ui_bridge.hpp"

#include "cyber/shared/net_socket.hpp"
#include "cyber/ui/websocket.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <utility>

namespace cyber::ui
{
namespace
{
std::string extract_string(const std::string& json, const std::string& key)
{
    const std::string marker = "\"" + key + "\":\"";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return "";
    }
    const std::size_t start = pos + marker.size();
    const std::size_t end = json.find('"', start);
    if (end == std::string::npos)
    {
        return "";
    }
    return json.substr(start, end - start);
}

int extract_int(const std::string& json, const std::string& key, int fallback)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return fallback;
    }
    return std::stoi(json.substr(pos + marker.size()));
}

float extract_float(const std::string& json, const std::string& key, float fallback)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return fallback;
    }
    return std::stof(json.substr(pos + marker.size()));
}

std::string json_escape(const std::string& value)
{
    std::string out;
    for (char ch : value)
    {
        if (ch == '"' || ch == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}
} // namespace

std::string login_state_json(const std::string& status, cyber::EntityId client_id,
                             const std::string& v_server, const std::string& message)
{
    std::string out = "{\"type\":\"loginState\",\"status\":\"" + json_escape(status) + "\"";
    if (cyber::is_client(client_id))
    {
        out += ",\"clientId\":" + std::to_string(static_cast<int>(client_id));
    }
    if (!v_server.empty())
    {
        out += ",\"vServer\":\"" + json_escape(v_server) + "\"";
    }
    if (!message.empty())
    {
        out += ",\"message\":\"" + json_escape(message) + "\"";
    }
    out += "}";
    return out;
}

std::string join_state_json(const std::string& status)
{
    return "{\"type\":\"joinState\",\"status\":\"" + json_escape(status) + "\"}";
}

UiBridge::UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler)
    : port_(port), self_(self), handler_(std::move(handler))
{
}

UiBridge::~UiBridge()
{
    stop();
    join_client_threads();
}

UiCommand UiBridge::parse_json_command(const std::string& text) const
{
    const std::string type = extract_string(text, "type");
    if (type == "login")
    {
        const int id = extract_int(text, "clientId", -1);
        cyber::EntityId client_id = cyber::EntityId::unknown;
        if (id >= 1 && id <= 4)
        {
            client_id = static_cast<cyber::EntityId>(id);
        }
        return {UiCommandKind::login, cyber::game::GameMsgType::error, {}, client_id,
                extract_string(text, "password")};
    }
    if (type == "join")
    {
        return {UiCommandKind::join_game, cyber::game::GameMsgType::error, {}, self_, ""};
    }
    if (type == "move")
    {
        const int x = std::max(-1, std::min(1, extract_int(text, "x", 0)));
        const int y = std::max(-1, std::min(1, extract_int(text, "y", 0)));
        return {UiCommandKind::game, cyber::game::GameMsgType::move,
                cyber::game::game_build_move({static_cast<std::int8_t>(x),
                                         static_cast<std::int8_t>(y)}),
                self_, ""};
    }
    if (type == "target")
    {
        return {UiCommandKind::game, cyber::game::GameMsgType::target,
                cyber::game::game_build_target({extract_float(text, "angle", 0.0F)}), self_, ""};
    }
    if (type == "shoot")
    {
        return {UiCommandKind::game, cyber::game::GameMsgType::shoot,
                cyber::game::game_build_shoot({}), self_, ""};
    }
    return {UiCommandKind::error, cyber::game::GameMsgType::error, {}, self_, ""};
}

void UiBridge::run()
{
    listener_ = cyber::listen_tcp({"127.0.0.1", port_});
    while (!stopping_)
    {
        try
        {
            cyber::SocketHandle accepted = cyber::accept_tcp(listener_);
            client_threads_.emplace_back(&UiBridge::handle_client, this, accepted);
        }
        catch (const std::exception&)
        {
            if (stopping_)
            {
                break;
            }
            throw;
        }
    }
    join_client_threads();
}

void UiBridge::handle_client(cyber::SocketHandle socket)
{
    try
    {
        perform_websocket_server_handshake(socket);
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.push_back(socket);
        }
        while (!stopping_)
        {
            const UiCommand command = parse_json_command(recv_ws_text(socket));
            if (command.kind != UiCommandKind::error && handler_)
            {
                handler_(command);
            }
        }
    }
    catch (const std::exception&)
    {
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(std::remove(clients_.begin(), clients_.end(), socket), clients_.end());
    }
    cyber::close_socket(socket);
}

void UiBridge::broadcast_state(const cyber::game::BattleStateSnapshot& snapshot)
{
    broadcast_text(cyber::game::game_format_state_json(snapshot, self_));
}

void UiBridge::set_self(cyber::EntityId self)
{
    self_ = self;
}

void UiBridge::broadcast_text(const std::string& json)
{
    std::vector<cyber::SocketHandle> clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients = clients_;
    }

    std::vector<cyber::SocketHandle> failed;
    for (cyber::SocketHandle client : clients)
    {
        try
        {
            send_ws_text(client, json);
        }
        catch (const std::exception&)
        {
            failed.push_back(client);
            cyber::close_socket(client);
        }
    }

    if (!failed.empty())
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (cyber::SocketHandle client : failed)
        {
            clients_.erase(std::remove(clients_.begin(), clients_.end(), client), clients_.end());
        }
    }
}

void UiBridge::stop()
{
    stopping_ = true;
    if (listener_ != 0)
    {
        cyber::close_socket(listener_);
        listener_ = 0;
    }
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (cyber::SocketHandle client : clients_)
    {
        cyber::close_socket(client);
    }
    clients_.clear();
}

void UiBridge::join_client_threads()
{
    for (std::thread& thread : client_threads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    client_threads_.clear();
}
} // namespace cyber::ui
