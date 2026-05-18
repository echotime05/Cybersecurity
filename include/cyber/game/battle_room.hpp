#pragma once

#include "cyber/game/game_protocol.hpp"
#include "cyber/game/game_types.hpp"
#include "cyber/game/game_world.hpp"

#include <array>
#include <map>
#include <mutex>
#include <vector>

namespace cyber::game
{
class BattleRoom
{
public:
    BattleRoom();

    bool join(EntityId client_id, std::uint64_t now_ms);
    void leave(EntityId client_id);
    void handle_move(EntityId client_id, std::int8_t x, std::int8_t y);
    void handle_target(EntityId client_id, float angle);
    void handle_shoot(EntityId client_id);
    void tick(std::uint64_t now_ms);
    BattleStateSnapshot snapshot(std::uint64_t now_ms) const;

private:
    struct TankItem : SpatialItem
    {
        TankState state;
    };

    struct BulletItem : SpatialItem
    {
        BulletState state;
    };

    struct PickableItem : SpatialItem
    {
        PickableState state;
    };

    std::uint8_t pick_weakest_team() const;
    void spawn_position(TankState& tank) const;
    void create_bullet(TankItem& tank, std::uint64_t now_ms);
    void rebuild_world();

    mutable std::mutex mutex_;
    std::array<TeamState, 4> teams_{};
    std::map<EntityId, TankItem> tanks_;
    std::map<std::uint16_t, BulletItem> bullets_;
    std::map<std::uint16_t, PickableItem> pickables_;
    std::vector<Block> blocks_;
    std::vector<PickableSpawn> pick_spawns_;
    World world_;
    std::uint16_t bullet_counter_ = 0;
    std::uint16_t pick_counter_ = 0;
    std::uint16_t total_score_ = 0;
    std::int8_t winner_team_ = -1;
    std::uint64_t winner_set_ms_ = 0;
};
} // namespace cyber::game
