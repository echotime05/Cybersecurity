#pragma once

#include "cyber/shared/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cyber::ui
{
// Web UI 发给 Client 的命令分类。
enum class UiCommandKind
{
    game,
    login,
    join_game,
    error
};

// Web UI 命令解析后的结构，Client 用它驱动登录、加入和游戏输入。
struct UiCommand
{
    UiCommandKind kind = UiCommandKind::error;
    cyber::game::GameMsgType type = cyber::game::GameMsgType::error;
    cyber::Bytes payload;
    cyber::EntityId client_id = cyber::EntityId::unknown;
    std::string password;
};

// 构造登录阶段状态 JSON，返回给 Web UI 更新登录页面。
std::string login_state_json(const std::string& status, cyber::EntityId client_id,
                             const std::string& v_server, const std::string& message);
// 构造加入游戏阶段状态 JSON。
std::string join_state_json(const std::string& status);

// Client 内置的 WebSocket 桥，负责 Web UI 和 C++ Client 之间的 JSON 通信。
class UiBridge
{
public:
    // UI 命令回调类型，由 TankGameClient 注入。
    using CommandHandler = std::function<void(const UiCommand&)>;

    // 在指定端口构造 WebSocket 桥。
    UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler);
    // 停止监听并回收客户端线程。
    ~UiBridge();

    // 阻塞运行 WebSocket 服务。
    void run();
    // 请求停止 WebSocket 服务。
    void stop();
    // 更新当前 Client 身份，登录成功后用于状态 JSON。
    void set_self(cyber::EntityId self);
    // 向所有连接的 Web UI 广播一段原始 JSON 文本。
    void broadcast_text(const std::string& json);
    // 将世界快照转换为 JSON 并广播给 Web UI。
    void broadcast_state(const cyber::game::BattleStateSnapshot& snapshot);
    // 解析 Web UI 发来的 JSON 命令。
    UiCommand parse_json_command(const std::string& text) const;

private:
    // 处理单个浏览器 WebSocket 连接。
    void handle_client(cyber::SocketHandle socket);
    // 等待所有浏览器连接线程结束。
    void join_client_threads();

    std::uint16_t port_ = 0;
    cyber::EntityId self_ = cyber::EntityId::unknown;
    CommandHandler handler_;
    cyber::SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex clients_mutex_;
    std::vector<cyber::SocketHandle> clients_;
    std::vector<std::thread> client_threads_;
};
} // namespace cyber::ui
