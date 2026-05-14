#include "cyber/common/config.hpp"
#include "cyber/game/auth_plain_game_client.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        cyber::Config config;
        cyber::game::AuthPlainGameClient plaintext(config, 7001, false);
        cyber::game::AuthPlainGameClient encrypted(config, 7002, true);
        (void)plaintext;
        (void)encrypted;
        std::cout << "auth_plain_game_client_options_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_plain_game_client_options_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
