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

// V 服务器内部保存的一辆坦克完整业务状态。
// TankState 不单独存放在 BattleRoom 中，而是作为 TankItem::state 存在。
// 它决定坦克的队伍、位置、生命值、输入、开火冷却、死亡和复活等游戏规则；
// BattleRoom 生成 GAME_STATE 时，也主要从这里取字段写入网络快照。
struct TankState
{
    // 控制这辆坦克的 Client ID。
    EntityId client_id = EntityId::unknown;
    // 坦克所属队伍编号。
    std::uint8_t team = 0;
    // 坦克当前世界坐标；tick 结束后由 TankItem 的物理坐标同步回来。
    float x = 0.0F;
    float y = 0.0F;
    // 炮塔朝向角度，由 GAME_TARGET 更新。
    float angle = 0.0F;
    // 当前生命值。
    std::int8_t hp = 10;
    // 当前护盾值，受到伤害时优先抵扣。
    std::int8_t shield = 0;
    // 是否处于死亡等待复活状态。
    bool dead = true;
    // 该坦克个人击杀得分。
    std::uint16_t score = 0;
    // 最近一次由 Client 上报的移动、瞄准和开火输入。
    GameInput input;
    // 是否处于开火冷却中。
    bool reloading = false;
    // 最近一次开火时间，用来判断 reload 是否结束。
    std::uint64_t last_shot_ms = 0;
    // 最近一次被子弹命中的时间。
    std::uint64_t hit_ms = 0;
    // 预留给自动恢复逻辑的时间字段，当前最终链路基本不依赖它。
    std::uint64_t recover_ms = 0;
    // 特殊弹药数量，damage 补给会增加该值。
    std::uint8_t ammo = 0;
    // 最近一次死亡时间，用来判断是否到达复活时间。
    std::uint64_t died_ms = 0;
    // 最近一次复活时间，用来判断复活后的短暂无敌期。
    std::uint64_t respawned_ms = 0;
    // 历史保留标记，当前最终链路没有实际使用。
    bool deleted = false;
};

// V 服务器内部保存的一颗子弹业务状态。
// BulletState 作为 BulletItem::state 存在；BulletItem 负责碰撞索引，
// BulletState 负责记录子弹归属、当前位置、目标点、速度、伤害和是否特殊子弹。
struct BulletState
{
    // 子弹唯一 ID，由 V 创建子弹时递增分配。
    std::uint16_t id = 0;
    // 发射这颗子弹的 Client ID，用于击杀归属和避免打到自己。
    EntityId owner = EntityId::unknown;
    // 发射者所在队伍，用于避免友军伤害。
    std::uint8_t owner_team = 0;
    // 子弹当前世界坐标。
    float x = 0.0F;
    float y = 0.0F;
    // 子弹飞行目标点；到达该点后子弹被移除。
    float target_x = 0.0F;
    float target_y = 0.0F;
    // 子弹每 tick 移动距离。
    float speed = kBulletSpeed;
    // 子弹命中造成的伤害。
    std::int8_t damage = kBulletDamage;
    // 是否为特殊子弹；特殊子弹由 ammo 消耗产生，伤害更高。
    bool special = false;
};

// V 服务器内部保存的一个已经刷出的补给物业务状态。
// PickableState 作为 PickableItem::state 存在；PickableSpawn 管刷新规则，
// PickableState 管当前地图上真实存在的补给物。
struct PickableState
{
    // 补给物唯一 ID，由 V 创建补给时递增分配。
    std::uint16_t id = 0;
    // 补给类型，例如回血、特殊弹药、护盾。
    PickableType type = PickableType::repair;
    // 补给物当前世界坐标。
    float x = 0.0F;
    float y = 0.0F;
    // 对应的 PickableSpawn 下标；被捡走后通过它找到刷新点并重置刷新计时。
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
