#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/roles/client/client_auth_flow.hpp"

#include <iostream>
#include <stdexcept>

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

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: encrypted_plaintext_rejection_client CONFIG\n";
        return 2;
    }

    try
    {
        cyber::SocketRuntime runtime;
        const cyber::Config config = cyber::Config::load(argv[1]);

        const cyber::EntityId client_id = cyber::EntityId::client1;
        const std::uint64_t kc = cyber::auth_derive_client_key(client_id, "123456");
        cyber::roles::client::VAuthenticatedSocket auth =
            cyber::roles::client::client_auth_connect_to_v_socket(config, client_id, kc);

        const cyber::Bytes plaintext_join = cyber::game::game_build_message(
            {cyber::game::GameMsgType::join,
             cyber::game::game_build_join({client_id})});

        cyber::send_packet_logged(auth.socket,
                                  cyber::make_packet(cyber::MsgType::app, client_id,
                                                     cyber::EntityId::v, plaintext_join));

        bool rejected = false;
        try
        {
            const cyber::Packet response =
                cyber::recv_packet_logged(auth.socket);
            if (response.msg_type != cyber::MsgType::app)
            {
                rejected = true;
            }
            else
            {
                try
                {
                    const cyber::Bytes decrypted =
                        cyber::game::app_decode_payload(response.payload, auth.state.kc_v, true);
                    const cyber::game::GameMessage message =
                        cyber::game::game_parse_message(decrypted);
                    rejected = message.type != cyber::game::GameMsgType::state;
                }
                catch (const std::exception&)
                {
                    rejected = true;
                }
            }
        }
        catch (const std::exception&)
        {
            rejected = true;
        }

        cyber::close_socket(auth.socket);
        require(rejected, "encrypted V accepted plaintext MsgType::app payload");
        std::cout << "encrypted_plaintext_rejection_client: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "encrypted_plaintext_rejection_client failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
