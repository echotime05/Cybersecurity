#include "cyber/shared/config.hpp"
#include "cyber/roles/client/tank_game_client.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        cyber::Config config;
        cyber::game::TankGameClient encrypted(config, 7002);
        (void)encrypted;
        std::cout << "tank_game_client_options_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "tank_game_client_options_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
