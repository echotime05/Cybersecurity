#include "cyber/game/battle_room.hpp"

#include <iostream>
#include <cmath>
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

void require_close(float actual, float expected, const char* message)
{
    if (std::fabs(actual - expected) > 0.0001F)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        require(cyber::game::kServerTickIntervalMs == 33U,
                "server tick interval should be 33ms");
        require_close(cyber::game::kTankSpeed, 0.2F,
                      "tank speed should use the 33ms baseline movement pace");
        require_close(cyber::game::kBulletSpeed, 0.65F,
                      "bullet speed should use the 33ms baseline movement pace");
        require_close(cyber::game::kTankRange, 32.0F,
                      "bullet range should be 32 world units");

        cyber::game::BattleRoom room;
        require(room.join(cyber::EntityId::client1, 1000),
                "client1 join failed");
        require(room.join(cyber::EntityId::client2, 1000),
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
        room.handle_shoot(cyber::EntityId::client1);
        room.tick(5500);
        state = room.snapshot(5500);
        require(!state.bullets.empty(), "shoot did not create bullet");
        const std::size_t bullets_after_shot = state.bullets.size();
        room.tick(6000);
        state = room.snapshot(6000);
        require(state.bullets.size() == bullets_after_shot,
                "shoot request should be consumed after one bullet");

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
