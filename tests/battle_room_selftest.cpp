#include "cyber/game/battle_room.hpp"

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
        cyber::game::BattleRoom room;
        require(room.join(cyber::EntityId::client1, "alpha", 1000),
                "client1 join failed");
        require(room.join(cyber::EntityId::client2, "bravo", 1000),
                "client2 join failed");
        auto state = room.snapshot(1000);
        require(state.tanks.size() == 2U, "join did not create two tanks");
        require(state.teams.size() == 4U, "team count mismatch");
        require(state.tanks[0].team != state.tanks[1].team,
                "first two tanks should split teams");

        room.handle_move(cyber::EntityId::client1, 1, 0);
        const float before_x = room.snapshot(1000).tanks[0].x;
        room.tick(5050);
        const float after_x = room.snapshot(5050).tanks[0].x;
        require(after_x > before_x, "move did not advance tank");

        room.handle_target(cyber::EntityId::client1, 90.0F);
        room.handle_shoot(cyber::EntityId::client1, true);
        room.tick(5500);
        state = room.snapshot(5500);
        require(!state.bullets.empty(), "shoot did not create bullet");

        room.leave(cyber::EntityId::client2);
        state = room.snapshot(5600);
        require(state.tanks.size() == 1U, "leave did not remove tank");

        std::cout << "battle_room_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "battle_room_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
