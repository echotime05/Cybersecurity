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

// 计算二维向量长度。
float length(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

// 计算两个坐标点之间的欧氏距离。
float distance(float ax, float ay, float bx, float by)
{
    return length(ax - bx, ay - by);
}

// 将 Client ID 转成 0 起始索引，用于出生点计算。
int client_index(EntityId id)
{
    return static_cast<int>(id) - static_cast<int>(EntityId::client1);
}

// 将空间对象限制在世界边界内。
void clamp_to_world(SpatialItem& item)
{
    item.x = std::max(item.radius, std::min(kWorldSize - item.radius, item.x));
    item.y = std::max(item.radius, std::min(kWorldSize - item.radius, item.y));
}
} // namespace

// 初始化战斗房间，加载固定墙块和补给刷新点。
BattleRoom::BattleRoom() : world_(kWorldSize, kWorldSize, kClusterSize)
{
    blocks_ = level_blocks();
    pick_spawns_ = pickable_spawns();
    rebuild_world();
}

// 选择当前人数最少、同人数时分数最低的队伍。
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

// 根据 Client ID 和队伍为坦克设置出生位置。
void BattleRoom::spawn_position(TankState& tank) const
{
    const int index = std::max(0, client_index(tank.client_id));
    tank.x = 2.5F + static_cast<float>(tank.team % 2U) * 35.0F +
             static_cast<float>((index * 3) % 9);
    tank.y = 2.5F + static_cast<float>(tank.team / 2U) * 35.0F +
             static_cast<float>((index * 5) % 9);
}

// 将 Client 加入房间，创建坦克并触发一次世界索引重建。
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

// 将 Client 从房间移除，并更新队伍人数。
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

// 更新某个 Client 的移动输入，方向值会被夹在 -1 到 1。
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

// 更新某个 Client 的炮塔角度，并规范到 0 到 360。
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

// 记录某个 Client 请求开火，真正创建子弹在 tick 中统一处理。
void BattleRoom::handle_shoot(EntityId client_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it != tanks_.end())
    {
        it->second.state.input.shoot_requested = true;
    }
}

// 根据坦克位置、角度和弹药状态创建子弹。
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

// 推进一帧战斗世界：补给刷新、坦克移动、碰撞、开火、子弹命中和胜利判定。
void BattleRoom::tick(std::uint64_t now_ms)
{
    // 步骤 1：锁住 BattleRoom 权威状态，保证网络线程写输入和游戏线程推进世界不会并发冲突。
    std::lock_guard<std::mutex> lock(mutex_);

    // 步骤 2：如果刚产生过胜者，保留短暂展示时间；展示结束后清空 winner_team_。
    if (winner_team_ >= 0 && now_ms - winner_set_ms_ >= 3000U)
    {
        winner_team_ = -1;
    }

    // 步骤 3：检查所有补给刷新点。如果该刷新点当前没有补给，且冷却时间已到，
    // 就创建新的 PickableItem，并把它放入当前地图补给表 pickables_。
    for (PickableSpawn& spawn : pick_spawns_)
    {
        if (spawn.active_id == 0 && now_ms - spawn.picked_ms > spawn.delay_ms)
        {
            // 步骤 3.1：为新补给分配非 0 ID，0 用作“当前没有活跃补给”的标记。
            PickableItem item;
            item.state.id = ++pick_counter_;
            if (item.state.id == 0)
            {
                item.state.id = ++pick_counter_;
            }
            // 步骤 3.2：把刷新点配置复制到补给业务状态和空间索引状态。
            item.state.type = spawn.type;
            item.state.x = spawn.x;
            item.state.y = spawn.y;
            item.state.spawn_index = &spawn - pick_spawns_.data();
            item.x = spawn.x;
            item.y = spawn.y;
            item.radius = kPickableRadius;
            // 步骤 3.3：刷新点记录当前活跃补给 ID，BattleRoom 记录补给实体。
            spawn.active_id = item.state.id;
            pickables_.emplace(item.state.id, item);
        }
    }

    // 步骤 4：逐个处理坦克的基础状态。
    // 死亡坦克到达复活时间后重生；存活坦克根据当前输入移动，并处理墙体碰撞。
    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        if (tank.state.dead)
        {
            // 步骤 4.1：死亡坦克先判断是否已经等待满复活时间。
            if (now_ms - tank.state.died_ms >= kRespawnTimeMs)
            {
                // 步骤 4.2：达到复活时间后，重置生命、护盾、弹药和开火冷却。
                tank.state.hp = 10;
                tank.state.shield = 0;
                tank.state.ammo = 0;
                tank.state.dead = false;
                tank.state.reloading = false;
                tank.state.respawned_ms = now_ms;
                // 步骤 4.3：重新分配出生点，并同步到用于碰撞计算的 TankItem 坐标。
                spawn_position(tank.state);
                tank.x = tank.state.x;
                tank.y = tank.state.y;
            }
            else
            {
                // 步骤 4.4：复活时间未到，本 tick 不再处理该死亡坦克。
                continue;
            }
        }

        // 步骤 4.5：把最近一次 GAME_TARGET 输入同步为坦克当前炮塔朝向。
        tank.state.angle = tank.state.input.angle;
        // 步骤 4.6：如果开火冷却时间已到，解除 reloading，允许后续再次开火。
        if (tank.state.reloading && now_ms - tank.state.last_shot_ms >= kReloadTimeMs)
        {
            tank.state.reloading = false;
        }

        // 步骤 4.7：读取最近一次 GAME_MOVE 输入，将方向归一化后推进坦克位置。
        const float dx = static_cast<float>(tank.state.input.dir_x);
        const float dy = static_cast<float>(tank.state.input.dir_y);
        const float len = length(dx, dy);
        if (len > 0.0001F)
        {
            tank.x += (dx / len) * kTankSpeed;
            tank.y += (dy / len) * kTankSpeed;
        }

        // 步骤 4.8：用圆形坦克和矩形墙块做碰撞检测；发生碰撞时把坦克推出墙外。
        for (const Block& block : blocks_)
        {
            if (const auto push = block.collide_circle(tank.x, tank.y, kTankRadius))
            {
                tank.x += push->x;
                tank.y += push->y;
            }
        }
        // 步骤 4.9：最后把坦克限制在世界边界内。
        clamp_to_world(tank);
    }

    // 步骤 5：处理坦克之间的圆形碰撞，防止多个坦克重叠。
    for (auto first = tanks_.begin(); first != tanks_.end(); ++first)
    {
        auto second = first;
        ++second;
        for (; second != tanks_.end(); ++second)
        {
            TankItem& a = first->second;
            TankItem& b = second->second;
            // 步骤 5.1：死亡坦克不参与坦克间挤压碰撞。
            if (a.state.dead || b.state.dead)
            {
                continue;
            }
            // 步骤 5.2：计算两个圆形坦克的距离，距离小于两个半径之和就说明重叠。
            const float dx = b.x - a.x;
            const float dy = b.y - a.y;
            const float dist = length(dx, dy);
            const float min_dist = kTankRadius * 2.0F;
            if (dist < min_dist)
            {
                // 步骤 5.3：两个坦克沿连线方向各推出一半重叠距离。
                const float nx = dist > 0.0001F ? dx / dist : 1.0F;
                const float ny = dist > 0.0001F ? dy / dist : 0.0F;
                const float push = (min_dist - dist) * 0.5F;
                a.x -= nx * push;
                a.y -= ny * push;
                b.x += nx * push;
                b.y += ny * push;
                // 步骤 5.4：推出后再次限制世界边界。
                clamp_to_world(a);
                clamp_to_world(b);
            }
        }
    }

    // 步骤 6：处理坦克拾取补给。
    // 坦克碰到补给后，根据补给类型回血、加特殊弹药或加护盾，同时重置刷新点计时。
    std::vector<std::uint16_t> picked_ids;
    for (auto& [tank_id, tank] : tanks_)
    {
        (void)tank_id;
        if (tank.state.dead)
        {
            // 步骤 6.1：死亡坦克不能拾取补给。
            continue;
        }
        for (auto& [pick_id, pickable] : pickables_)
        {
            // 步骤 6.2：用圆形距离判断坦克是否碰到补给。
            if (distance(tank.x, tank.y, pickable.x, pickable.y) >= kTankRadius + kPickableRadius)
            {
                continue;
            }
            // 步骤 6.3：根据补给类型修改坦克状态。
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
            // 步骤 6.4：通知原刷新点“当前补给已被拾取”，并记录拾取时间用于后续刷新。
            if (pickable.state.spawn_index < pick_spawns_.size())
            {
                PickableSpawn& spawn = pick_spawns_[pickable.state.spawn_index];
                spawn.active_id = 0;
                spawn.picked_ms = now_ms;
            }
            // 步骤 6.5：记录被拾取的补给 ID，循环结束后统一从 pickables_ 删除。
            picked_ids.push_back(pick_id);
        }
    }
    // 步骤 6.6：删除本 tick 已被拾取的补给实体。
    for (std::uint16_t id : picked_ids)
    {
        pickables_.erase(id);
    }

    // 步骤 7：消费一次性开火输入。
    // GAME_SHOOT 只把 shoot_requested 置为 true，真正能不能生成子弹由这里统一判断。
    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        // 步骤 7.1：读取本 tick 是否收到过 GAME_SHOOT，并立即清空，保证一次请求只消费一次。
        const bool shoot_requested = tank.state.input.shoot_requested;
        tank.state.input.shoot_requested = false;
        // 步骤 7.2：只有存活、收到开火请求、且不在 reload 冷却中的坦克才能创建子弹。
        if (!tank.state.dead && shoot_requested && !tank.state.reloading)
        {
            create_bullet(tank, now_ms);
        }
    }

    // 步骤 8：推进所有子弹。
    // 子弹按目标点飞行；到达终点、越界、撞墙或命中坦克后都会被加入删除列表。
    std::vector<std::uint16_t> remove_bullets;
    for (auto& [bullet_id, bullet] : bullets_)
    {
        // 步骤 8.1：计算子弹当前位置到目标点的方向和剩余距离。
        const float dx = bullet.state.target_x - bullet.x;
        const float dy = bullet.state.target_y - bullet.y;
        const float len = length(dx, dy);
        if (len <= bullet.state.speed || len <= 0.0001F)
        {
            // 步骤 8.2：子弹到达最大射程目标点，标记删除。
            remove_bullets.push_back(bullet_id);
            continue;
        }
        // 步骤 8.3：沿目标方向推进子弹，并同步回 BulletState。
        bullet.x += (dx / len) * bullet.state.speed;
        bullet.y += (dy / len) * bullet.state.speed;
        bullet.state.x = bullet.x;
        bullet.state.y = bullet.y;

        // 步骤 8.4：子弹越出世界边界，标记删除。
        if (bullet.x < 0.0F || bullet.x > kWorldSize || bullet.y < 0.0F || bullet.y > kWorldSize)
        {
            remove_bullets.push_back(bullet_id);
            continue;
        }
        bool consumed = false;
        // 步骤 8.5：子弹撞到任意矩形墙块，标记删除。
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
            // 步骤 8.6：跳过死亡目标、同队目标以及发射者自己。
            if (target.state.dead || target.state.team == bullet.state.owner_team ||
                tank_id == bullet.state.owner)
            {
                continue;
            }
            // 步骤 8.7：刚复活的坦克处于短暂无敌期，不能被命中。
            if (now_ms - target.state.respawned_ms < kInvulnerableTimeMs)
            {
                continue;
            }
            // 步骤 8.8：用圆形距离判断子弹是否命中目标坦克。
            if (distance(bullet.x, bullet.y, target.x, target.y) >= kTankRadius + kBulletRadius)
            {
                continue;
            }

            // 步骤 8.9：子弹命中敌方坦克后，先扣护盾，再扣生命值。
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
                // 步骤 8.10：坦克生命值归零后进入死亡状态，并给子弹发射者记分。
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
                            // 步骤 8.11：队伍达到胜利分数后，记录胜者并重置各队和坦克分数。
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
            // 步骤 8.12：子弹命中任意有效目标后，本颗子弹消耗并退出目标遍历。
            remove_bullets.push_back(bullet_id);
            break;
        }
    }
    // 步骤 8.13：统一删除本 tick 已消耗或失效的子弹。
    for (std::uint16_t id : remove_bullets)
    {
        bullets_.erase(id);
    }

    // 步骤 9：把物理对象坐标同步回 TankState，保证后续 GAME_STATE 快照使用最新位置。
    for (auto& [id, tank] : tanks_)
    {
        (void)id;
        tank.state.x = tank.x;
        tank.state.y = tank.y;
    }
    // 步骤 10：根据本 tick 后的最新坦克、子弹、补给和墙体，重建下一帧使用的空间索引。
    rebuild_world();
}

// 生成可广播给 Client 的完整世界快照。
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

// 根据当前对象集合重建空间索引，保证碰撞查询使用最新位置。
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
