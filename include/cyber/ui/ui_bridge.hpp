#pragma once

#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cyber::ui
{
enum class UiCommandKind
{
    game,
    login,
    join_game,
    error
};

struct UiCommand
{
    UiCommandKind kind = UiCommandKind::error;
    cyber::game::GameMsgType type = cyber::game::GameMsgType::error;
    cyber::Bytes payload;
    cyber::EntityId client_id = cyber::EntityId::unknown;
    std::string password;
};

std::string login_state_json(const std::string& status, cyber::EntityId client_id,
                             const std::string& v_server, const std::string& message);
std::string join_state_json(const std::string& status);

class UiBridge
{
public:
    using CommandHandler = std::function<void(const UiCommand&)>;

    UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler);
    ~UiBridge();

    void run();
    void stop();
    void set_self(cyber::EntityId self);
    void broadcast_text(const std::string& json);
    void broadcast_state(const cyber::game::BattleStateSnapshot& snapshot);
    UiCommand parse_json_command(const std::string& text) const;

private:
    void handle_client(cyber::SocketHandle socket);
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
