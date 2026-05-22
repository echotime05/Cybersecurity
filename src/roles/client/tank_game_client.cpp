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
    // 步骤 1：登录命令进入 Kerberos + V 认证流程。
    if (command.kind == cyber::ui::UiCommandKind::login)
    {
        handle_login(command);
    }
    // 步骤 2：加入游戏命令要求已经认证成功，随后向 V 发送 GAME_JOIN_REQ。
    else if (command.kind == cyber::ui::UiCommandKind::join_game)
    {
        handle_join();
    }
    // 步骤 3：移动、瞄准、开火命令统一转成加密签名的 MSG_APP 发给 V。
    else if (command.kind == cyber::ui::UiCommandKind::game)
    {
        handle_game_command(command);
    }
}

// 处理登录命令：派生 Kc，执行认证流程，并启动 V 接收线程。
void TankGameClient::handle_login(const cyber::ui::UiCommand& command)
{
    // 步骤 1：先做本地输入校验，避免空密码或非法 Client ID 进入认证流程。
    if (!is_client(command.client_id) || command.password.empty())
    {
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
        return;
    }

    {
        // 步骤 2：把状态切到 authenticating，防止用户连续点击 Login 造成并发认证。
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
        // 步骤 3：如果之前已经连过 V，先关闭旧 socket 并等待旧接收线程退出。
        close_v_socket();
        if (rx_thread_.joinable())
        {
            rx_thread_.join();
        }

        // 步骤 4：由真实密码派生 Client 长期密钥 Kc，并执行 AS -> TGS -> V 的认证链路。
        // 成功后会得到已认证的 V socket、C-V 会话密钥 Kc_v、Client 证书密钥和 V 公钥。
        const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
        cyber::roles::client::VAuthenticatedSocket auth =
            cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
        {
            // 步骤 5：把认证结果保存为后续游戏发包、收包和签名验签需要的运行状态。
            std::lock_guard<std::mutex> lock(state_mutex_);
            self_ = command.client_id;
            kc_v_ = auth.state.kc_v;
            client_key_pair_ = auth.state.client_key_pair;
            v_public_key_ = auth.state.v_public_key;
            v_socket_ = auth.socket;
            state_ = State::authenticated;
        }
        // 步骤 6：认证成功后启动 V 接收线程，并通知 Web UI 进入 authenticated 状态。
        bridge_->set_self(self_);
        rx_thread_ = std::thread([this]() { receive_loop(); });
        bridge_->broadcast_text(cyber::ui::login_state_json("authenticated", self_,
                                                            v_server_text(), ""));
    }
    catch (const std::exception&)
    {
        // 步骤 7：认证失败时清理 socket、密钥和状态，只向 UI 暴露统一的登录失败结果。
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
        // 步骤 1：只有认证成功但尚未加入游戏时，才允许发送加入房间请求。
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::authenticated)
        {
            return;
        }
        client = self_;
        state_ = State::joined;
    }
    // 步骤 2：构造 GAME_JOIN_REQ 的应用层 payload，并通过安全游戏通道发送给 V。
    send_game_message(GameMsgType::join, game_build_join({client}));
    // 步骤 3：本地 UI 切到 joined；真正的世界状态仍以后续 V 广播的 GAME_STATE 为准。
    bridge_->broadcast_text(cyber::ui::join_state_json("joined"));
}

// 处理 Web UI 的移动、瞄准和开火命令，未加入游戏时忽略。
void TankGameClient::handle_game_command(const cyber::ui::UiCommand& command)
{
    {
        // 步骤 1：只有已经加入战斗房间后，移动、瞄准和开火命令才会被转发。
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::joined)
        {
            return;
        }
    }
    // 步骤 2：Client 不在本地修改权威世界，只把用户输入封装后发给 V。
    send_game_message(command.type, command.payload);
}

// 浏览器输入在这里变成签名加密的游戏 MSG_APP；浏览器仍只渲染 V 返回的权威 GAME_STATE。
void TankGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    // 步骤 1：所有发往 V 的游戏报文共用一个发送锁，避免普通游戏报文和 ACK 交叉写 socket。
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    // 步骤 2：把 app_code + game payload 组成 SignedAppPayload，用 Client 私钥签名，再用 Kc_v 加密。
    const Packet packet =
        app_build_signed_game_packet(self_, EntityId::v, type, payload, kc_v_,
                                     client_key_pair_.private_key);
    // 步骤 3：把网络包写入 V socket；Header 明文保留，payload 是加密后的应用层内容。
    send_packet_logged(v_socket_, packet);
    // 步骤 4：写入 Protocol UI 事件，同时保存 payload 明文视图和网络密文视图。
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
            // 步骤 1：阻塞等待 V 发来的下一个 Packet；连接关闭或读取失败会抛异常退出循环。
            const Packet packet = recv_packet_logged(v_socket_);
            if (packet.msg_type != MsgType::app)
            {
                continue;
            }
            // 步骤 2：MSG_APP 的 payload 先用 Kc_v 解密，得到带签名的 SignedAppPayload。
            const SignedAppPayload signed_payload = app_decode_signed_packet(packet, kc_v_);
            if (signed_payload.app_code == AppCode::app_ack)
            {
                // 步骤 3：ACK 是对本端已发送报文的不可否认证据；只记录并验签，不再回复 ACK。
                write_protocol_event(ProtocolDirection::recv, packet,
                                     protocol_app_message(AppCode::app_ack),
                                     app_build_payload_view(packet, kc_v_));
                (void)ack_parse_verified_payload(signed_payload, v_public_key_);
                continue;
            }

            // 步骤 4：非 ACK 的应用层游戏报文必须通过 V 公钥验签，验签成功后才能解析业务内容。
            const GameMessage message =
                app_parse_verified_game_message(signed_payload, v_public_key_);
            write_protocol_event(ProtocolDirection::recv, packet,
                                 protocol_app_message(signed_payload.app_code),
                                 app_build_payload_view(packet, kc_v_));
            // 步骤 5：对收到的非 ACK 报文生成 APP_ACK，ACK 内引用原报文摘要并由 Client 私钥签名。
            const Packet ack = ack_build_signed_packet(packet, signed_payload, self_, EntityId::v,
                                                       kc_v_, client_key_pair_.private_key);
            {
                // 步骤 6：通过同一把发送锁发送 ACK，并把 ACK 完整写入 Protocol UI 事件日志。
                std::lock_guard<std::mutex> lock(send_mutex_);
                if (v_socket_ != 0)
                {
                    send_packet_logged(v_socket_, ack);
                    write_protocol_event(ProtocolDirection::send, ack,
                                         protocol_app_message(AppCode::app_ack),
                                         app_build_payload_view(ack, kc_v_));
                }
            }
            // 步骤 7：只有 GAME_STATE 会被转成 Web UI 可渲染的世界状态；其它报文只进入日志。
            if (message.type == GameMsgType::state && bridge_)
            {
                bridge_->broadcast_state(game_parse_state(message.payload));
            }
        }
    }
    catch (const std::exception&)
    {
        // 步骤 8：接收线程异常通常表示 V 连接断开，通知 Web UI 回到失败状态。
        if (!stopping_)
        {
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
