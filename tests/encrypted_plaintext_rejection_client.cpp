#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_protocol.hpp"

#include <filesystem>
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
        cyber::Logger logger(std::filesystem::temp_directory_path() /
                             "encrypted_plaintext_rejection_client.log");

        const cyber::EntityId client_id = cyber::EntityId::client1;
        const std::uint64_t kc = cyber::derive_client_key(client_id, "123456");
        cyber::VAuthenticatedSocket auth = cyber::authenticate_client_to_v_socket(
            config, client_id, kc, logger, "EncryptedPlainReject");

        const cyber::Bytes plaintext_join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({client_id})});

        cyber::send_packet_logged(auth.socket,
                                  cyber::make_packet(cyber::MsgType::app, client_id,
                                                     cyber::EntityId::v, plaintext_join),
                                  logger, "Client", "EncryptedPlainReject");

        bool rejected = false;
        try
        {
            const cyber::Packet response =
                cyber::recv_packet_logged(auth.socket, logger, "Client",
                                          "EncryptedPlainReject");
            if (response.msg_type != cyber::MsgType::app)
            {
                rejected = true;
            }
            else
            {
                try
                {
                    const cyber::Bytes decrypted =
                        cyber::game::decode_app_payload(response.payload, auth.state.kc_v, true);
                    const cyber::game::GameMessage message =
                        cyber::game::parse_game_message(decrypted);
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
