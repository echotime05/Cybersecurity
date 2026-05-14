#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"
#include "cyber/common/config.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/game/plain_game_server.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
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

cyber::Config make_test_config()
{
    const std::filesystem::path config_path =
        std::filesystem::temp_directory_path() / "plain_game_flow_config.txt";
    std::ofstream out(config_path);
    out << "C1_ID=0x01\nC2_ID=0x02\nC3_ID=0x03\nC4_ID=0x04\n"
        << "AS_ID=0x11\nTGS_ID=0x12\nV_ID=0x13\nLOCAL_CLIENT_ID=0x01\n"
        << "AS_BIND_IP=127.0.0.1\nAS_IP=127.0.0.1\nAS_HOST=127.0.0.1\nAS_PORT=1\n"
        << "TGS_BIND_IP=127.0.0.1\nTGS_IP=127.0.0.1\nTGS_HOST=127.0.0.1\nTGS_PORT=2\n"
        << "V_BIND_IP=127.0.0.1\nV_IP=127.0.0.1\nV_HOST=127.0.0.1\nV_PORT=3\n"
        << "C1_PASSWORD=123456\nC1_KC=0x59ef3db7cb8c8d\n"
        << "C2_PASSWORD=admin123\nC2_KC=0x6a73a4ebe9c564\n"
        << "C3_PASSWORD=hehe12345\nC3_KC=0x57ef9d5f45ab7b\n"
        << "C4_PASSWORD=&wxh@147\nC4_KC=0xec3eb766d59086\n"
        << "KTGS=0x1c24deeecc136e\nKV=0x3398481d2a89f6\n"
        << "PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3\n"
        << "PK_CA_E=0x10001\n"
        << "SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69\n";
    out.close();
    return cyber::Config::load(config_path);
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

        const cyber::Config config = make_test_config();
        cyber::game::PlainGameServer auth_server({"127.0.0.1", 0}, config, true);
        const std::uint16_t auth_port = auth_server.start_for_test();
        std::thread auth_thread([&]() { auth_server.run_until_stopped(); });

        cyber::SocketHandle unauthenticated = cyber::connect_tcp({"127.0.0.1", auth_port});
        const auto bad_join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "Client1"})});
        cyber::send_packet_logged(unauthenticated,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     bad_join),
                                  client_logger, "Client", "AuthGateTest");
        bool closed_or_failed = false;
        try
        {
            (void)cyber::recv_packet_logged(unauthenticated, client_logger, "Client",
                                            "AuthGateTest");
        }
        catch (const std::exception&)
        {
            closed_or_failed = true;
        }
        require(closed_or_failed, "auth-gated server accepted app traffic before V_AUTH");
        cyber::close_socket(unauthenticated);
        auth_server.stop();
        auth_thread.join();

        std::cout << "plain_game_flow_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "plain_game_flow_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
