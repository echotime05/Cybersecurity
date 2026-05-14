#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/ui/ui_bridge.hpp"

#include <atomic>
#include <memory>
#include <mutex>

namespace cyber::game
{
class PlainGameClient
{
public:
    PlainGameClient(Config config, std::uint16_t ui_port);
    void run();

private:
    void send_game_message(GameMsgType type, const Bytes& payload);
    void receive_loop();
    void handle_ui_command(const cyber::ui::UiCommand& command);

    Config config_;
    EntityId self_ = EntityId::unknown;
    std::uint16_t ui_port_ = 0;
    SocketHandle v_socket_ = 0;
    std::mutex send_mutex_;
    std::atomic<bool> stopping_{false};
    Logger logger_;
    std::unique_ptr<cyber::ui::UiBridge> bridge_;
};
} // namespace cyber::game
