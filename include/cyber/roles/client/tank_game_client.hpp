#pragma once

#include "cyber/shared/config.hpp"
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
// Client 进程主类：连接 Web UI，执行登录认证，和 V 进行加密游戏报文交互。
class TankGameClient
{
public:
    // 使用配置和本地 UI 端口构造 Client。
    TankGameClient(Config config, std::uint16_t ui_port);
    // 停止 UI/V 连接并回收接收线程。
    ~TankGameClient();

    // 阻塞运行 Client，启动本地 WebSocket UI 并等待用户操作。
    void run();

private:
    // Client 页面和网络链路的阶段状态。
    enum class State
    {
        waiting_for_login,
        authenticating,
        authenticated,
        joined
    };

    // 处理 Web UI 发来的任意命令。
    void handle_ui_command(const cyber::ui::UiCommand& command);
    // 处理登录命令，完成 AS/TGS/V 认证。
    void handle_login(const cyber::ui::UiCommand& command);
    // 登录成功后发送 GAME_JOIN_REQ 加入战斗房间。
    void handle_join();
    // 处理移动、瞄准、开火等游戏命令。
    void handle_game_command(const cyber::ui::UiCommand& command);
    // 后台接收 V 发来的游戏状态和 ACK。
    void receive_loop();
    // 构造签名加密 MSG_APP 并发送到 V。
    void send_game_message(GameMsgType type, const Bytes& payload);
    // 关闭当前 V socket 并重置句柄。
    void close_v_socket();
    // 生成页面展示使用的 V 地址文本。
    std::string v_server_text() const;

    Config config_;
    std::uint16_t ui_port_ = 0;
    EntityId self_ = EntityId::unknown;
    SocketHandle v_socket_ = 0;
    std::uint64_t kc_v_ = 0;
    RsaKeyPair client_key_pair_;
    RsaPublicKey v_public_key_;
    State state_ = State::waiting_for_login;
    std::mutex state_mutex_;
    std::mutex send_mutex_;
    std::atomic<bool> stopping_{false};
    std::unique_ptr<cyber::ui::UiBridge> bridge_;
    std::thread rx_thread_;
};
} // namespace cyber::game
