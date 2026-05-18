#pragma once

#include "cyber/common/config.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/roles/client/client_auth_flow.hpp"
#include "cyber/ui/ui_bridge.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cyber::game
{
class TankGameClient
{
public:
    TankGameClient(Config config, std::uint16_t ui_port, bool encrypt_app_payloads = false);
    ~TankGameClient();

    void run();

private:
    enum class State
    {
        waiting_for_login,
        authenticating,
        authenticated,
        joined
    };

    void handle_ui_command(const cyber::ui::UiCommand& command);
    void handle_login(const cyber::ui::UiCommand& command);
    void handle_join();
    void handle_game_command(const cyber::ui::UiCommand& command);
    void receive_loop();
    void send_game_message(GameMsgType type, const Bytes& payload);
    void close_v_socket();
    std::string v_server_text() const;

    Config config_;
    std::uint16_t ui_port_ = 0;
    EntityId self_ = EntityId::unknown;
    SocketHandle v_socket_ = 0;
    std::uint64_t kc_v_ = 0;
    RsaKeyPair client_key_pair_;
    RsaPublicKey v_public_key_;
    bool encrypt_app_payloads_ = false;
    State state_ = State::waiting_for_login;
    std::mutex state_mutex_;
    std::mutex send_mutex_;
    std::atomic<bool> stopping_{false};
    std::unique_ptr<cyber::ui::UiBridge> bridge_;
    std::thread rx_thread_;
};
} // namespace cyber::game
