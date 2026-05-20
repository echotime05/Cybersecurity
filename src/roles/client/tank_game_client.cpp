#include "cyber/roles/client/tank_game_client.hpp"

#include "cyber/shared/auth_credentials.hpp"
#include "cyber/shared/net_packet.hpp"
#include "cyber/protocol/protocol_event.hpp"
#include "cyber/shared/runtime_paths.hpp"
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_non_repudiation.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace cyber::game
{
namespace
{
// 为已加密 MSG_APP 构造协议可视化 payload 明文/密文对照。
ProtocolPayloadView app_build_payload_view(const Packet& packet, std::uint64_t kc_v)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(app_decode_payload(packet.payload, kc_v));
    view.encrypted_hex = bytes_to_hex(packet.payload);
    return view;
}
} // namespace

// 构造 Client 主对象，保存配置和本地 UI 端口。
TankGameClient::TankGameClient(Config config, std::uint16_t ui_port)
    : config_(std::move(config)),
      ui_port_(ui_port)
{
    set_protocol_event_log_root(log_root_from_config(config_));
}

// 停止 Client：关闭 V socket、停止 UI 桥并等待接收线程结束。
TankGameClient::~TankGameClient()
{
    stopping_ = true;
    close_v_socket();
    if (bridge_)
    {
        bridge_->stop();
    }
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

// 运行 Client，本地启动 WebSocket UI 桥并等待浏览器操作。
void TankGameClient::run()
{
    SocketRuntime runtime;
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, EntityId::unknown,
        [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });

    std::cout << "Client tank game UI ws://127.0.0.1:" << ui_port_
              << " mode=auth-encrypted\n";
    bridge_->broadcast_text(cyber::ui::login_state_json("idle", EntityId::unknown, "", ""));
    bridge_->run();
    stopping_ = true;
    close_v_socket();
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

// 分发 Web UI 命令到登录、加入或游戏输入处理函数。
void TankGameClient::handle_ui_command(const cyber::ui::UiCommand& command)
{
    if (command.kind == cyber::ui::UiCommandKind::login)
    {
        handle_login(command);
    }
    else if (command.kind == cyber::ui::UiCommandKind::join_game)
    {
        handle_join();
    }
    else if (command.kind == cyber::ui::UiCommandKind::game)
    {
        handle_game_command(command);
    }
}

// 处理登录命令：派生 Kc，执行认证流程，并启动 V 接收线程。
void TankGameClient::handle_login(const cyber::ui::UiCommand& command)
{
    if (!is_client(command.client_id) || command.password.empty())
    {
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == State::authenticating)
        {
            return;
        }
        state_ = State::authenticating;
    }
    bridge_->broadcast_text(
        cyber::ui::login_state_json("authenticating", command.client_id, "", ""));

    try
    {
        close_v_socket();
        if (rx_thread_.joinable())
        {
            rx_thread_.join();
        }

        const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
        cyber::roles::client::VAuthenticatedSocket auth =
            cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            self_ = command.client_id;
            kc_v_ = auth.state.kc_v;
            client_key_pair_ = auth.state.client_key_pair;
            v_public_key_ = auth.state.v_public_key;
            v_socket_ = auth.socket;
            state_ = State::authenticated;
        }
        bridge_->set_self(self_);
        rx_thread_ = std::thread([this]() { receive_loop(); });
        bridge_->broadcast_text(cyber::ui::login_state_json("authenticated", self_,
                                                            v_server_text(), ""));
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Client auth failed: " << ex.what() << '\n';
        close_v_socket();
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            kc_v_ = 0;
            client_key_pair_ = {};
            v_public_key_ = {};
            state_ = State::waiting_for_login;
        }
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
    }
}

// 处理加入游戏命令：发送 GAME_JOIN_REQ 并切换到 joined 状态。
void TankGameClient::handle_join()
{
    EntityId client = EntityId::unknown;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::authenticated)
        {
            return;
        }
        client = self_;
        state_ = State::joined;
    }
    send_game_message(GameMsgType::join, game_build_join({client}));
    bridge_->broadcast_text(cyber::ui::join_state_json("joined"));
}

// 处理 Web UI 的移动、瞄准和开火命令，未加入游戏时忽略。
void TankGameClient::handle_game_command(const cyber::ui::UiCommand& command)
{
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::joined)
        {
            return;
        }
    }
    send_game_message(command.type, command.payload);
}

// 浏览器输入在这里变成签名加密的游戏 MSG_APP；浏览器仍只渲染 V 返回的权威 GAME_STATE。
void TankGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    const Packet packet =
        app_build_signed_game_packet(self_, EntityId::v, type, payload, kc_v_,
                                     client_key_pair_.private_key);
    send_packet_logged(v_socket_, packet);
    write_protocol_event(ProtocolDirection::send, packet,
                         protocol_app_message(app_map_game_message_code(type)),
                         app_build_payload_view(packet, kc_v_));
}

// 接收循环处理 ACK 证据和权威状态；V 发来的非 ACK 游戏包会回复签名 APP_ACK。
void TankGameClient::receive_loop()
{
    try
    {
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(v_socket_);
            if (packet.msg_type != MsgType::app)
            {
                continue;
            }
            const SignedAppPayload signed_payload = app_decode_signed_packet(packet, kc_v_);
            if (signed_payload.app_code == AppCode::app_ack)
            {
                write_protocol_event(ProtocolDirection::recv, packet,
                                     protocol_app_message(AppCode::app_ack),
                                     app_build_payload_view(packet, kc_v_));
                (void)ack_parse_verified_payload(signed_payload, v_public_key_);
                continue;
            }

            const GameMessage message =
                app_parse_verified_game_message(signed_payload, v_public_key_);
            write_protocol_event(ProtocolDirection::recv, packet,
                                 protocol_app_message(signed_payload.app_code),
                                 app_build_payload_view(packet, kc_v_));
            const Packet ack = ack_build_signed_packet(packet, signed_payload, self_, EntityId::v,
                                                       kc_v_, client_key_pair_.private_key);
            {
                std::lock_guard<std::mutex> lock(send_mutex_);
                if (v_socket_ != 0)
                {
                    send_packet_logged(v_socket_, ack);
                    write_protocol_event(ProtocolDirection::send, ack,
                                         protocol_app_message(AppCode::app_ack),
                                         app_build_payload_view(ack, kc_v_));
                }
            }
            if (message.type == GameMsgType::state && bridge_)
            {
                bridge_->broadcast_state(game_parse_state(message.payload));
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (!stopping_)
        {
            std::cerr << "Client receive failed: " << ex.what() << '\n';
            bridge_->broadcast_text(cyber::ui::login_state_json(
                "failed", EntityId::unknown, "", "V connection closed"));
        }
    }
}

// 关闭当前连接 V 的 socket，并把句柄清零。
void TankGameClient::close_v_socket()
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
}

// 返回登录成功后 UI 展示的 V 服务地址。
std::string TankGameClient::v_server_text() const
{
    return config_.get_string("V_IP") + ":" + std::to_string(config_.get_u16("V_PORT"));
}
} // namespace cyber::game
