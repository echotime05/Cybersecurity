#pragma once

#include "cyber/shared/types.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstddef>
#include <cstdint>

namespace cyber::game
{
// V 服务器的世界更新周期，当前为 20ms 一帧。
constexpr std::uint64_t kServerTickIntervalMs = 20;
// 将原 33ms 参数按当前 tick 周期缩放，保持速度体感接近。
constexpr float kTickScaleFrom33Ms = static_cast<float>(kServerTickIntervalMs) / 33.0F;
// 坦克每 tick 移动距离。
constexpr float kTankSpeed = 0.2F * kTickScaleFrom33Ms;
// 坦克和子弹的世界活动范围半径。
constexpr float kTankRange = 32.0F;
// 坦克碰撞半径。
constexpr float kTankRadius = 0.75F;
// 子弹每 tick 移动距离。
constexpr float kBulletSpeed = 0.65F * kTickScaleFrom33Ms;
// 子弹碰撞半径。
constexpr float kBulletRadius = 0.25F;
// 普通子弹命中造成的基础伤害。
constexpr std::int8_t kBulletDamage = 3;
// 补给物碰撞半径。
constexpr float kPickableRadius = 0.3F;
// 坦克死亡后的复活等待时间。
constexpr std::uint64_t kRespawnTimeMs = 5000;
// 坦克复活后的无敌时间。
constexpr std::uint64_t kInvulnerableTimeMs = 2000;
// 开火冷却时间，用于限制连续发射频率。
constexpr std::uint64_t kReloadTimeMs = 400;
// 受伤后开始自动恢复的等待时间。
constexpr std::uint64_t kRecoveryDelayMs = 3000;
// 自动恢复的间隔时间。
constexpr std::uint64_t kRecoveryIntervalMs = 1000;
// 队伍达到该分数后判定胜利。
constexpr std::uint16_t kWinScore = 10;

// 二维坐标或位移向量。
struct Vec2
{
    float x = 0.0F;
    float y = 0.0F;
};

// V 服务器保存的一个玩家当前输入状态。
struct GameInput
{
    std::int8_t dir_x = 0;
    std::int8_t dir_y = 0;
    float angle = 0.0F;
    bool shoot_requested = false;
};

// 队伍当前得分和存活坦克数量。
struct TeamState
{
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

// V 服务器内部保存的一辆坦克完整运行状态。
struct TankState
{
    EntityId client_id = EntityId::unknown;
    std::uint8_t team = 0;
    float x = 0.0F;
    float y = 0.0F;
    float angle = 0.0F;
    std::int8_t hp = 10;
    std::int8_t shield = 0;
    bool dead = true;
    std::uint16_t score = 0;
    GameInput input;
    bool reloading = false;
    std::uint64_t last_shot_ms = 0;
    std::uint64_t hit_ms = 0;
    std::uint64_t recover_ms = 0;
    std::uint8_t ammo = 0;
    std::uint64_t died_ms = 0;
    std::uint64_t respawned_ms = 0;
    bool deleted = false;
};

// V 服务器内部保存的一颗子弹状态。
struct BulletState
{
    std::uint16_t id = 0;
    EntityId owner = EntityId::unknown;
    std::uint8_t owner_team = 0;
    float x = 0.0F;
    float y = 0.0F;
    float target_x = 0.0F;
    float target_y = 0.0F;
    float speed = kBulletSpeed;
    std::int8_t damage = kBulletDamage;
    bool special = false;
};

// V 服务器内部保存的一个补给物状态。
struct PickableState
{
    std::uint16_t id = 0;
    PickableType type = PickableType::repair;
    float x = 0.0F;
    float y = 0.0F;
    std::size_t spawn_index = 0;
};

// 地图中一个补给刷新点的配置和运行状态。
struct PickableSpawn
{
    float x = 0.0F;
    float y = 0.0F;
    PickableType type = PickableType::repair;
    std::uint64_t delay_ms = 0;
    std::uint64_t picked_ms = 0;
    std::uint16_t active_id = 0;
};
} // namespace cyber::game
