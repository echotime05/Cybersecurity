#include "cyber/game/game_protocol.hpp"
#include "cyber/protocol/packet.hpp"
#include "cyber/roles/v/tank_game_server.hpp"
#include "cyber/shared/config.hpp"
#include "cyber/shared/net_packet.hpp"
#include "cyber/shared/net_socket.hpp"

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
        std::filesystem::temp_directory_path() / "tank_game_server_flow_config.txt";
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
        const cyber::Config config = make_test_config();
        cyber::game::TankGameServer server({"127.0.0.1", 0}, config);
        const std::uint16_t port = server.start_for_test();
        std::thread server_thread([&]() { server.run_until_stopped(); });

        cyber::SocketHandle unauthenticated = cyber::connect_tcp({"127.0.0.1", port});
        const cyber::Bytes join_payload = cyber::game::game_build_message(
            {cyber::game::GameMsgType::join,
             cyber::game::game_build_join({cyber::EntityId::client1})});
        cyber::send_packet_logged(unauthenticated,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     join_payload));

        bool closed_or_failed = false;
        try
        {
            (void)cyber::recv_packet_logged(unauthenticated);
        }
        catch (const std::exception&)
        {
            closed_or_failed = true;
        }
        require(closed_or_failed, "V accepted MSG_APP before V_AUTH");

        cyber::close_socket(unauthenticated);
        server.stop();
        server_thread.join();

        std::cout << "tank_game_server_flow_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "tank_game_server_flow_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
