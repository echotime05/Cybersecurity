#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/game/plain_game_server.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        cyber::SocketRuntime runtime;
        cyber::game::PlainGameServer server({"127.0.0.1", 0});
        const std::uint16_t port = server.start_for_test();
        std::thread server_thread([&]() { server.run_until_stopped(); });

        cyber::Logger client_logger(std::filesystem::temp_directory_path() /
                                    "plain_game_flow_client.log");
        cyber::SocketHandle client = cyber::connect_tcp({"127.0.0.1", port});
        const auto join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "alpha"})});
        cyber::send_packet_logged(client,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     join),
                                  client_logger, "Client", "PlainGameTest");
        bool saw_state = false;
        for (int i = 0; i < 10 && !saw_state; ++i)
        {
            const cyber::Packet packet =
                cyber::recv_packet_logged(client, client_logger, "Client", "PlainGameTest");
            const cyber::game::GameMessage message = cyber::game::parse_game_message(packet.payload);
            if (message.type == cyber::game::GameMsgType::state)
            {
                const cyber::game::BattleStateSnapshot state =
                    cyber::game::parse_state(message.payload);
                saw_state = !state.tanks.empty();
            }
        }
        require(saw_state, "did not receive state with joined tank");

        cyber::close_socket(client);
        server.stop();
        server_thread.join();
        std::cout << "plain_game_flow_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "plain_game_flow_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
