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
// 从简单 JSON 文本里提取字符串字段，当前 UI 命令格式固定所以不用完整 JSON 库。
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

// 从简单 JSON 文本里提取整数字段，缺失时返回 fallback。
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

// 从简单 JSON 文本里提取浮点字段，缺失时返回 fallback。
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

// 转义 JSON 字符串里的引号和反斜杠。
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

// 构造登录状态 JSON，推送给登录页更新按钮和错误提示。
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

// 构造加入游戏状态 JSON，推送给 UI 切换游戏页面。
std::string join_state_json(const std::string& status)
{
    return "{\"type\":\"joinState\",\"status\":\"" + json_escape(status) + "\"}";
}

// 构造 UI 桥，保存监听端口、当前 Client 身份和命令回调。
UiBridge::UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler)
    : port_(port), self_(self), handler_(std::move(handler))
{
}

// 停止 UI 桥并等待浏览器连接线程结束。
UiBridge::~UiBridge()
{
    stop();
    join_client_threads();
}

// 解析浏览器发来的 JSON 命令，转换成 C++ Client 可处理的 UiCommand。
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

// 启动本地 WebSocket 监听，并为每个浏览器连接创建处理线程。
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

// 处理单个浏览器 WebSocket 连接，持续接收命令并回调给 Client。
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

// 将 V 发来的世界快照转换为 JSON 并广播给浏览器。
void UiBridge::broadcast_state(const cyber::game::BattleStateSnapshot& snapshot)
{
    broadcast_text(cyber::game::game_format_state_json(snapshot, self_));
}

// 更新 UI 桥记录的当前 Client ID。
void UiBridge::set_self(cyber::EntityId self)
{
    self_ = self;
}

// 向所有已连接浏览器广播原始 JSON 文本，失败连接会被移除。
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

// 停止监听并关闭所有浏览器连接。
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

// 等待全部浏览器处理线程结束。
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
