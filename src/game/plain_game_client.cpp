#include "cyber/game/plain_game_client.hpp"

#include "cyber/common/net_packet.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace cyber::game
{
namespace
{
TcpEndpoint v_endpoint(const Config& config)
{
    return {config.get_string("V_IP"), config.get_u16("V_PORT")};
}
} // namespace

PlainGameClient::PlainGameClient(Config config, std::uint16_t ui_port)
    : config_(std::move(config)),
      self_(config_.get_entity_id("LOCAL_CLIENT_ID")),
      ui_port_(ui_port),
      logger_(std::filesystem::path("logs") / "client_plain_game.log")
{
}

void PlainGameClient::run()
{
    SocketRuntime runtime;
    v_socket_ = connect_tcp(v_endpoint(config_));
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, self_, [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });

    std::thread rx([this]() { receive_loop(); });
    std::thread ui([this]() { bridge_->run(); });

    const std::string default_name = "player" + std::to_string(static_cast<int>(self_));
    send_game_message(GameMsgType::join, build_join({self_, default_name}));
    std::cout << "Client legacy tank game connected to V. UI ws://127.0.0.1:" << ui_port_
              << '\n';

    ui.join();
    stopping_ = true;
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
    if (rx.joinable())
    {
        rx.join();
    }
}

void PlainGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    const Bytes message = build_game_message({type, payload});
    const Packet packet = make_packet(MsgType::app, self_, EntityId::v, message);
    send_packet_logged(v_socket_, packet, logger_, "Client", "PlainGameTx");
}

void PlainGameClient::handle_ui_command(const cyber::ui::UiCommand& command)
{
    if (command.type == GameMsgType::error)
    {
        return;
    }
    send_game_message(command.type, command.payload);
}

void PlainGameClient::receive_loop()
{
    try
    {
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(v_socket_, logger_, "Client", "PlainGameRx");
            if (packet.msg_type != MsgType::app)
            {
                continue;
            }
            const GameMessage message = parse_game_message(packet.payload);
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
            logger_.write("Client", "PlainGameRx", "ERROR", ex.what());
            stopping_ = true;
            if (bridge_)
            {
                bridge_->stop();
            }
        }
    }
}
} // namespace cyber::game
