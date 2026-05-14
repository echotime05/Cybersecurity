#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstdint>
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

int main()
{
    try
    {
        const std::uint64_t kc_v = 0x123456789abcdeULL;
        const cyber::Bytes plain = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "Client1"})});

        const cyber::Bytes identity =
            cyber::game::encode_app_payload(plain, kc_v, false);
        require(identity == plain, "plaintext codec should not change payload");
        require(cyber::game::decode_app_payload(identity, kc_v, false) == plain,
                "plaintext codec should roundtrip");

        const cyber::Bytes cipher = cyber::game::encode_app_payload(plain, kc_v, true);
        require(cipher != plain, "encrypted codec should change payload bytes");
        require(cipher.size() % 8U == 0U, "encrypted codec should produce DES blocks");

        const cyber::Bytes decoded = cyber::game::decode_app_payload(cipher, kc_v, true);
        require(decoded == plain, "encrypted codec should roundtrip");

        const cyber::game::GameMessage parsed = cyber::game::parse_game_message(decoded);
        require(parsed.type == cyber::game::GameMsgType::join,
                "decoded game message type mismatch");

        bool plaintext_rejected_by_encrypted_decoder = false;
        try
        {
            (void)cyber::game::decode_app_payload(plain, kc_v, true);
        }
        catch (const std::exception&)
        {
            plaintext_rejected_by_encrypted_decoder = true;
        }
        require(plaintext_rejected_by_encrypted_decoder,
                "encrypted decoder accepted plaintext payload");

        std::cout << "app_payload_codec_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "app_payload_codec_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
