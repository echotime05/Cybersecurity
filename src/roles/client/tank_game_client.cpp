#include "cyber/roles/client/tank_game_client.hpp"

#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/protocol_event.hpp"
#include "cyber/common/runtime_paths.hpp"
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
ProtocolPayloadView app_build_payload_view(const Packet& packet, std::uint64_t kc_v, bool encrypted)
{
    ProtocolPayloadView view;
    if (encrypted)
    {
        view.plain_hex = bytes_to_hex(app_decode_payload(packet.payload, kc_v, true));
        view.encrypted_hex = bytes_to_hex(packet.payload);
    }
    else
    {
        view.plain_hex = bytes_to_hex(packet.payload);
    }
    return view;
}
} // namespace

TankGameClient::TankGameClient(Config config, std::uint16_t ui_port,
                               bool encrypt_app_payloads)
    : config_(std::move(config)),
      ui_port_(ui_port),
      encrypt_app_payloads_(encrypt_app_payloads)
{
    set_protocol_event_log_root(log_root_from_config(config_));
}

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

void TankGameClient::run()
{
    SocketRuntime runtime;
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, EntityId::unknown,
        [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });

    const char* mode = encrypt_app_payloads_ ? "encrypted" : "auth-plain";
    std::cout << "Client tank game UI ws://127.0.0.1:" << ui_port_ << " mode=" << mode
              << '\n';
    bridge_->broadcast_text(cyber::ui::login_state_json("idle", EntityId::unknown, "", ""));
    bridge_->run();
    stopping_ = true;
    close_v_socket();
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

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

// Browser input becomes a signed game MSG_APP here. The browser still renders
// only the authoritative GAME_STATE that V sends back.
void TankGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    const Packet packet =
        app_build_signed_game_packet(self_, EntityId::v, type, payload, kc_v_, encrypt_app_payloads_,
                                 client_key_pair_.private_key);
    send_packet_logged(v_socket_, packet);
    write_protocol_event(ProtocolDirection::send, packet,
                         protocol_app_message(app_map_game_message_code(type)),
                         app_build_payload_view(packet, kc_v_, encrypt_app_payloads_));
}

// The receive loop handles ACK evidence and authoritative state messages.
// Non-ACK game packets from V are acknowledged with signed APP_ACK.
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
            const SignedAppPayload signed_payload =
                app_decode_signed_packet(packet, kc_v_, encrypt_app_payloads_);
            if (signed_payload.app_code == AppCode::app_ack)
            {
                write_protocol_event(ProtocolDirection::recv, packet,
                                     protocol_app_message(AppCode::app_ack),
                                     app_build_payload_view(packet, kc_v_, encrypt_app_payloads_));
                (void)ack_parse_verified_payload(signed_payload, v_public_key_);
                continue;
            }

            const GameMessage message =
                app_parse_verified_game_message(signed_payload, v_public_key_);
            write_protocol_event(ProtocolDirection::recv, packet,
                                 protocol_app_message(signed_payload.app_code),
                                 app_build_payload_view(packet, kc_v_, encrypt_app_payloads_));
            const Packet ack = ack_build_signed_packet(packet, signed_payload, self_, EntityId::v,
                                                       kc_v_, encrypt_app_payloads_,
                                                       client_key_pair_.private_key);
            {
                std::lock_guard<std::mutex> lock(send_mutex_);
                if (v_socket_ != 0)
                {
                    send_packet_logged(v_socket_, ack);
                    write_protocol_event(ProtocolDirection::send, ack,
                                         protocol_app_message(AppCode::app_ack),
                                         app_build_payload_view(ack, kc_v_, encrypt_app_payloads_));
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

void TankGameClient::close_v_socket()
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
}

std::string TankGameClient::v_server_text() const
{
    return config_.get_string("V_IP") + ":" + std::to_string(config_.get_u16("V_PORT"));
}
} // namespace cyber::game
