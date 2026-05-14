#include "cyber/game/game_world.hpp"

#include <cmath>
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
        const cyber::game::Block block(5.0F, 5.0F, 2.0F, 2.0F);
        const auto push = block.collide_circle(4.2F, 5.0F, 0.75F);
        require(push.has_value(), "expected block collision");
        require(push->x < 0.0F, "expected push out on x axis");

        cyber::game::World world(48.0F, 48.0F, 4.0F);
        cyber::game::SpatialItem a;
        a.x = 2.0F;
        a.y = 2.0F;
        a.radius = 0.75F;
        cyber::game::SpatialItem b;
        b.x = 3.0F;
        b.y = 2.0F;
        b.radius = 0.75F;
        world.add(cyber::game::SpatialLayer::tank, &a);
        world.add(cyber::game::SpatialLayer::tank, &b);

        int seen = 0;
        world.for_each_around(cyber::game::SpatialLayer::tank, a,
                              [&](cyber::game::SpatialItem& item) {
                                  if (&item == &b)
                                  {
                                      ++seen;
                                  }
                              },
                              &a);
        require(seen == 1, "world did not find nearby tank");

        a.x = -10.0F;
        a.y = 100.0F;
        world.update(cyber::game::SpatialLayer::tank, a);
        require_close(a.x, a.radius, "world did not clamp x");
        require_close(a.y, 48.0F - a.radius, "world did not clamp y");

        require(cyber::game::level_blocks().size() == 40U, "level block count mismatch");
        require(cyber::game::pickable_spawns().size() == 9U, "pickable spawn count mismatch");

        std::cout << "game_world_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "game_world_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
