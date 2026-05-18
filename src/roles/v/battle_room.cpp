#include "cyber/game/battle_room.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace cyber::game
{
namespace
{
constexpr float kWorldSize = 48.0F;
constexpr float kClusterSize = 4.0F;
constexpr float kPi = 3.14159265358979323846F;

float length(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

float distance(float ax, float ay, float bx, float by)
{
    return length(ax - bx, ay - by);
}

int client_index(EntityId id)
{
    return static_cast<int>(id) - static_cast<int>(EntityId::client1);
}

void clamp_to_world(SpatialItem& item)
{
    item.x = std::max(item.radius, std::min(kWorldSize - item.radius, item.x));
    item.y = std::max(item.radius, std::min(kWorldSize - item.radius, item.y));
}
} // namespace

BattleRoom::BattleRoom() : world_(kWorldSize, kWorldSize, kClusterSize)
{
    blocks_ = level_blocks();
    pick_spawns_ = pickable_spawns();
    rebuild_world();
}

std::uint8_t BattleRoom::pick_weakest_team() const
{
    std::uint8_t best = 0;
    for (std::uint8_t i = 1; i < teams_.size(); ++i)
    {
        if (teams_[i].tanks < teams_[best].tanks ||
            (teams_[i].tanks == teams_[best].tanks && teams_[i].score < teams_[best].score))
        {
            best = i;
        }
    }
    return best;
}

void BattleRoom::spawn_position(TankState& tank) const
{
    const int index = std::max(0, client_index(tank.client_id));
    tank.x = 2.5F + static_cast<float>(tank.team % 2U) * 35.0F +
             static_cast<float>((index * 3) % 9);
    tank.y = 2.5F + static_cast<float>(tank.team / 2U) * 35.0F +
             static_cast<float>((index * 5) % 9);
}

bool BattleRoom::join(EntityId client_id, std::uint64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_client(client_id) || tanks_.count(client_id) != 0U)
    {
        return false;
    }
    const std::uint8_t team = pick_weakest_team();
    TankItem item;
    item.state.client_id = client_id;
    item.state.team = team;
    item.state.dead = true;
    item.state.died_ms = now_ms >= kRespawnTimeMs ? now_ms - kRespawnTimeMs : 0;
    item.state.respawned_ms = now_ms;
    spawn_position(item.state);
    item.x = item.state.x;
    item.y = item.state.y;
    item.radius = kTankRadius;
    teams_[team].tanks++;
    tanks_.emplace(client_id, item);
    rebuild_world();
    return true;
}

void BattleRoom::leave(EntityId client_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tanks_.find(client_id);
    if (it == tanks_.end())
    {
        return;
    }
    if (it->second.state.team < teams_.size() && teams_[it->second.state.team].tanks > 0)
    {
        teams_[it->second.state.team].tanks--;
    }
    tanks_.erase(it);
    rebuild_world();
}

void BattleRoom::handle_move(EntityId client_id, std::int8_t x, std::int8_t y)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it == tanks_.end())
    {
        return;
    }
    it->second.state.input.dir_x =
        static_cast<std::int8_t>(std::max(-1, std::min(1, static_cast<int>(x))));
    it->second.state.input.dir_y =
        static_cast<std::int8_t>(std::max(-1, std::min(1, static_cast<int>(y))));
}

void BattleRoom::handle_target(EntityId client_id, float angle)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it != tanks_.end() && std::isfinite(angle))
    {
        it->second.state.input.angle = std::fmod(std::fmod(angle, 360.0F) + 360.0F, 360.0F);
        it->second.state.angle = it->second.state.input.angle;
    }
}

void BattleRoom::handle_shoot(EntityId client_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it != tanks_.end())
    {
        it->second.state.input.shoot_requested = true;
    }
}

void BattleRoom::create_bullet(TankItem& tank, std::uint64_t now_ms)
{
    tank.state.reloading = true;
    tank.state.last_shot_ms = now_ms;

    const float radians = tank.state.angle * kPi / 180.0F;
    BulletItem bullet;
    bullet.state.id = ++bullet_counter_;
    if (bullet.state.id == 0)
    {
        bullet.state.id = ++bullet_counter_;
    }
    bullet.state.owner = tank.state.client_id;
    bullet.state.owner_team = tank.state.team;
    bullet.state.x = tank.x + std::cos(radians) * (kTankRadius + kBulletRadius);
    bullet.state.y = tank.y + std::sin(radians) * (kTankRadius + kBulletRadius);
    bullet.state.target_x = tank.x + std::cos(radians) * kTankRange;
    bullet.state.target_y = tank.y + std::sin(radians) * kTankRange;
    bullet.state.special = tank.state.ammo > 0;
    if (bullet.state.special)
    {
        bullet.state.damage = static_cast<std::int8_t>(kBulletDamage + 2);
        --tank.state.ammo;
    }
    bullet.x = bullet.state.x;
    bullet.y = bullet.state.y;
    bullet.radius = kBulletRadius;
    bullets_.emplace(bullet.state.id, bullet);
}

void BattleRoom::tick(std::uint64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (winner_team_ >= 0 && now_ms - winner_set_ms_ >= 3000U)
    {
        winner_team_ = -1;
    }

    for (PickableSpawn& spawn : pick_spawns_)
    {
        if (spawn.active_id == 0 && now_ms - spawn.picked_ms > spawn.delay_ms)
        {
            PickableItem item;
            item.state.id = ++pick_counter_;
            if (item.state.id == 0)
            {
                item.state.id = ++pick_counter_;
            }
            item.state.type = spawn.type;
            item.state.x = spawn.x;
            item.state.y = spawn.y;
            item.state.spawn_index = &spawn - pick_spawns_.data();
            item.x = spawn.x;
            item.y = spawn.y;
            item.radius = kPickableRadius;
            spawn.active_id = item.state.id;
            pickables_.emplace(item.state.id, item);
        }
    }

    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        if (tank.state.dead)
        {
            if (now_ms - tank.state.died_ms >= kRespawnTimeMs)
            {
                tank.state.hp = 10;
                tank.state.shield = 0;
                tank.state.ammo = 0;
                tank.state.dead = false;
                tank.state.reloading = false;
                tank.state.respawned_ms = now_ms;
                spawn_position(tank.state);
                tank.x = tank.state.x;
                tank.y = tank.state.y;
            }
            else
            {
                continue;
            }
        }

        tank.state.angle = tank.state.input.angle;
        if (tank.state.reloading && now_ms - tank.state.last_shot_ms >= kReloadTimeMs)
        {
            tank.state.reloading = false;
        }

        const float dx = static_cast<float>(tank.state.input.dir_x);
        const float dy = static_cast<float>(tank.state.input.dir_y);
        const float len = length(dx, dy);
        if (len > 0.0001F)
        {
            tank.x += (dx / len) * kTankSpeed;
            tank.y += (dy / len) * kTankSpeed;
        }

        for (const Block& block : blocks_)
        {
            if (const auto push = block.collide_circle(tank.x, tank.y, kTankRadius))
            {
                tank.x += push->x;
                tank.y += push->y;
            }
        }
        clamp_to_world(tank);
    }

    for (auto first = tanks_.begin(); first != tanks_.end(); ++first)
    {
        auto second = first;
        ++second;
        for (; second != tanks_.end(); ++second)
        {
            TankItem& a = first->second;
            TankItem& b = second->second;
            if (a.state.dead || b.state.dead)
            {
                continue;
            }
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float dist = length(dx, dy);
            const float min_dist = kTankRadius * 2.0F;
            if (dist < min_dist)
            {
                const float nx = dist > 0.0001F ? dx / dist : 1.0F;
                const float ny = dist > 0.0001F ? dy / dist : 0.0F;
                const float push = (min_dist - dist) * 0.5F;
                a.x -= nx * push;
                a.y -= ny * push;
                b.x += nx * push;
                b.y += ny * push;
                clamp_to_world(a);
                clamp_to_world(b);
            }
        }
    }

    std::vector<std::uint16_t> picked_ids;
    for (auto& [tank_id, tank] : tanks_)
    {
        (void)tank_id;
        if (tank.state.dead)
        {
            continue;
        }
        for (auto& [pick_id, pickable] : pickables_)
        {
            if (distance(tank.x, tank.y, pickable.x, pickable.y) >= kTankRadius + kPickableRadius)
            {
                continue;
            }
            if (pickable.state.type == PickableType::repair)
            {
                tank.state.hp = static_cast<std::int8_t>(std::min<int>(10, tank.state.hp + 4));
            }
            else if (pickable.state.type == PickableType::damage)
            {
                tank.state.ammo = static_cast<std::uint8_t>(std::min<int>(3, tank.state.ammo + 1));
            }
            else if (pickable.state.type == PickableType::shield)
            {
                tank.state.shield =
                    static_cast<std::int8_t>(std::min<int>(10, tank.state.shield + 5));
            }
            if (pickable.state.spawn_index < pick_spawns_.size())
            {
                PickableSpawn& spawn = pick_spawns_[pickable.state.spawn_index];
                spawn.active_id = 0;
                spawn.picked_ms = now_ms;
            }
            picked_ids.push_back(pick_id);
        }
    }
    for (std::uint16_t id : picked_ids)
    {
        pickables_.erase(id);
    }

    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        const bool shoot_requested = tank.state.input.shoot_requested;
        tank.state.input.shoot_requested = false;
        if (!tank.state.dead && shoot_requested && !tank.state.reloading)
        {
            create_bullet(tank, now_ms);
        }
    }

    std::vector<std::uint16_t> remove_bullets;
    for (auto& [bullet_id, bullet] : bullets_)
    {
        const float dx = bullet.state.target_x - bullet.x;
        const float dy = bullet.state.target_y - bullet.y;
        const float len = length(dx, dy);
        if (len <= bullet.state.speed || len <= 0.0001F)
        {
            remove_bullets.push_back(bullet_id);
            continue;
        }
        bullet.x += (dx / len) * bullet.state.speed;
        bullet.y += (dy / len) * bullet.state.speed;
        bullet.state.x = bullet.x;
        bullet.state.y = bullet.y;

        if (bullet.x < 0.0F || bullet.x > kWorldSize || bullet.y < 0.0F || bullet.y > kWorldSize)
        {
            remove_bullets.push_back(bullet_id);
            continue;
        }
        bool consumed = false;
        for (const Block& block : blocks_)
        {
            if (block.collide_circle(bullet.x, bullet.y, kBulletRadius))
            {
                remove_bullets.push_back(bullet_id);
                consumed = true;
                break;
            }
        }
        if (consumed)
        {
            continue;
        }

        for (auto& [tank_id, target] : tanks_)
        {
            if (target.state.dead || target.state.team == bullet.state.owner_team ||
                tank_id == bullet.state.owner)
            {
                continue;
            }
            if (now_ms - target.state.respawned_ms < kInvulnerableTimeMs)
            {
                continue;
            }
            if (distance(bullet.x, bullet.y, target.x, target.y) >= kTankRadius + kBulletRadius)
            {
                continue;
            }

            std::int8_t remaining = bullet.state.damage;
            if (target.state.shield > 0)
            {
                const std::int8_t absorbed = std::min(target.state.shield, remaining);
                target.state.shield = static_cast<std::int8_t>(target.state.shield - absorbed);
                remaining = static_cast<std::int8_t>(remaining - absorbed);
            }
            if (remaining > 0)
            {
                target.state.hp = static_cast<std::int8_t>(target.state.hp - remaining);
            }
            target.state.hit_ms = now_ms;
            if (target.state.hp <= 0)
            {
                target.state.hp = 0;
                target.state.dead = true;
                target.state.died_ms = now_ms;
                target.state.input.shoot_requested = false;
                target.state.reloading = false;
                auto owner = tanks_.find(bullet.state.owner);
                if (owner != tanks_.end())
                {
                    ++owner->second.state.score;
                    if (owner->second.state.team < teams_.size())
                    {
                        ++teams_[owner->second.state.team].score;
                        if (teams_[owner->second.state.team].score >= kWinScore)
                        {
                            winner_team_ = static_cast<std::int8_t>(owner->second.state.team);
                            winner_set_ms_ = now_ms;
                            for (TeamState& team : teams_)
                            {
                                team.score = 0;
                            }
                            for (auto& [reset_id, reset_tank] : tanks_)
                            {
                                (void)reset_id;
                                reset_tank.state.score = 0;
                                reset_tank.state.dead = true;
                                reset_tank.state.died_ms = now_ms;
                            }
                        }
                    }
                    ++total_score_;
                }
            }
            remove_bullets.push_back(bullet_id);
            break;
        }
    }
    for (std::uint16_t id : remove_bullets)
    {
        bullets_.erase(id);
    }

    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        tank.state.x = tank.x;
        tank.state.y = tank.y;
    }
    rebuild_world();
}

BattleStateSnapshot BattleRoom::snapshot(std::uint64_t now_ms) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    BattleStateSnapshot snapshot;
    snapshot.server_time_ms = now_ms;
    snapshot.total_score = total_score_;
    snapshot.winner_team = winner_team_;
    for (std::size_t i = 0; i < teams_.size(); ++i)
    {
        snapshot.teams.push_back(
            {static_cast<std::uint8_t>(i), teams_[i].score, teams_[i].tanks});
    }
    for (const auto& [id, tank] : tanks_)
    {
        (void)id;
        snapshot.tanks.push_back({tank.state.client_id,
                                  tank.state.team,
                                  tank.state.x,
                                  tank.state.y,
                                  tank.state.angle,
                                  tank.state.hp,
                                  tank.state.shield,
                                  tank.state.dead,
                                  tank.state.score});
    }
    for (const auto& [id, bullet] : bullets_)
    {
        (void)id;
        snapshot.bullets.push_back({bullet.state.id, bullet.state.owner, bullet.state.x,
                                    bullet.state.y, bullet.state.special});
    }
    for (const auto& [id, pickable] : pickables_)
    {
        (void)id;
        snapshot.pickables.push_back(
            {pickable.state.id, pickable.state.type, pickable.state.x, pickable.state.y});
    }
    return snapshot;
}

void BattleRoom::rebuild_world()
{
    world_ = World(kWorldSize, kWorldSize, kClusterSize);
    for (Block& block : blocks_)
    {
        world_.add(SpatialLayer::block, &block);
    }
    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        tank.radius = kTankRadius;
        tank.x = tank.state.x;
        tank.y = tank.state.y;
        world_.add(SpatialLayer::tank, &tank);
    }
    for (auto& [id, bullet] : bullets_)
    {
        (void)id;
        bullet.radius = kBulletRadius;
        world_.add(SpatialLayer::bullet, &bullet);
    }
    for (auto& [id, pickable] : pickables_)
    {
        (void)id;
        pickable.radius = kPickableRadius;
        world_.add(SpatialLayer::pickable, &pickable);
    }
}
} // namespace cyber::game
