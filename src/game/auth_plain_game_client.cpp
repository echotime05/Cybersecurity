#include "cyber/game/auth_plain_game_client.hpp"

#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/game/app_payload_codec.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace cyber::game
{
AuthPlainGameClient::AuthPlainGameClient(Config config, std::uint16_t ui_port,
                                         bool encrypt_app_payloads)
    : config_(std::move(config)),
      ui_port_(ui_port),
      encrypt_app_payloads_(encrypt_app_payloads),
      logger_(std::filesystem::path("logs") / "client_auth_plain_game.log")
{
}

AuthPlainGameClient::~AuthPlainGameClient()
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

void AuthPlainGameClient::run()
{
    SocketRuntime runtime;
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, EntityId::unknown,
        [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });

    std::cout << "Client auth plaintext game UI ws://127.0.0.1:" << ui_port_ << '\n';
    bridge_->broadcast_text(cyber::ui::login_state_json("idle", EntityId::unknown, "", ""));
    bridge_->run();
    stopping_ = true;
    close_v_socket();
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

void AuthPlainGameClient::handle_ui_command(const cyber::ui::UiCommand& command)
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

void AuthPlainGameClient::handle_login(const cyber::ui::UiCommand& command)
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
    bridge_->broadcast_text(cyber::ui::login_state_json("authenticating", command.client_id, "", ""));

    try
    {
        close_v_socket();
        if (rx_thread_.joinable())
        {
            rx_thread_.join();
        }

        const std::uint64_t kc = derive_client_key(command.client_id, command.password);
        VAuthenticatedSocket auth =
            authenticate_client_to_v_socket(config_, command.client_id, kc, logger_,
                                            "AuthPlainGameClient");
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            self_ = command.client_id;
            kc_v_ = auth.state.kc_v;
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
        logger_.write("Client", "AuthPlainGameClient", "ERROR", ex.what());
        close_v_socket();
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            kc_v_ = 0;
            state_ = State::waiting_for_login;
        }
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
    }
}

void AuthPlainGameClient::handle_join()
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
    send_game_message(GameMsgType::join, build_join({client, default_client_name(client)}));
    bridge_->broadcast_text(cyber::ui::join_state_json("joined"));
}

void AuthPlainGameClient::handle_game_command(const cyber::ui::UiCommand& command)
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

void AuthPlainGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    const Bytes message = build_game_message({type, payload});
    const Bytes wire_payload = encode_app_payload(message, kc_v_, encrypt_app_payloads_);
    const Packet packet = make_packet(MsgType::app, self_, EntityId::v, wire_payload);
    send_packet_logged(v_socket_, packet, logger_, "Client", "AuthPlainGameTx");
}

void AuthPlainGameClient::receive_loop()
{
    try
    {
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(v_socket_, logger_, "Client",
                                                     "AuthPlainGameRx");
            if (packet.msg_type != MsgType::app)
            {
                continue;
            }
            const Bytes plain_payload =
                decode_app_payload(packet.payload, kc_v_, encrypt_app_payloads_);
            const GameMessage message = parse_game_message(plain_payload);
            if (message.type == GameMsgType::state && bridge_)
            {
                bridge_->broadcast_state(parse_state(message.payload));
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (!stopping_)
        {
            logger_.write("Client", "AuthPlainGameRx", "ERROR", ex.what());
            bridge_->broadcast_text(cyber::ui::login_state_json(
                "failed", EntityId::unknown, "", "V connection closed"));
        }
    }
}

void AuthPlainGameClient::close_v_socket()
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
}

std::string AuthPlainGameClient::v_server_text() const
{
    return config_.get_string("V_IP") + ":" + std::to_string(config_.get_u16("V_PORT"));
}
} // namespace cyber::game
