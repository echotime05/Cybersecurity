#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cyber::game
{
// 游戏层内部消息类型，位于 MSG_APP 的 signed app_payload 内。
enum class GameMsgType : std::uint8_t
{
    join = 1,
    move = 2,
    target = 3,
    shoot = 4,
    state = 16,
    error = 127
};

// 地图补给类型，影响坦克回血、加伤害或加护盾。
enum class PickableType : std::uint8_t
{
    repair = 1,
    damage = 2,
    shield = 3
};

// 游戏层通用消息封装：先放 GameMsgType，再放对应消息 payload。
struct GameMessage
{
    GameMsgType type = GameMsgType::error;
    Bytes payload;
};

// Client 加入战斗房间的消息，仅携带自己的 Client ID。
struct JoinMessage
{
    EntityId client_id = EntityId::unknown;
};

// Client 移动输入消息，x/y 取 -1、0、1 表示方向轴。
struct MoveMessage
{
    std::int8_t x = 0;
    std::int8_t y = 0;
};

// Client 炮塔朝向消息，angle 是 Web UI 计算出的朝向弧度。
struct TargetMessage
{
    float angle = 0.0F;
};

// Client 单次开火消息，当前点击一次触发一次，不再携带布尔状态。
struct ShootMessage
{};

// 快照里的队伍状态，用于 UI 计分板。
struct TeamSnapshot
{
    std::uint8_t team_id = 0;
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

// 快照里一辆坦克的可渲染状态。
struct TankSnapshot
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
};

// 快照里一颗子弹的可渲染状态。
struct BulletSnapshot
{
    std::uint16_t id = 0;
    EntityId owner_client_id = EntityId::unknown;
    float x = 0.0F;
    float y = 0.0F;
    bool special = false;
};

// 快照里一个补给物的可渲染状态。
struct PickableSnapshot
{
    std::uint16_t id = 0;
    PickableType type = PickableType::repair;
    float x = 0.0F;
    float y = 0.0F;
};

// V 周期广播给 Client 的完整世界快照。
struct BattleStateSnapshot
{
    std::uint64_t server_time_ms = 0;
    std::uint16_t total_score = 0;
    std::int8_t winner_team = -1;
    std::vector<TeamSnapshot> teams;
    std::vector<TankSnapshot> tanks;
    std::vector<BulletSnapshot> bullets;
    std::vector<PickableSnapshot> pickables;
};

// 序列化游戏层通用消息。
Bytes game_build_message(const GameMessage& message);
// 解析游戏层通用消息。
GameMessage game_parse_message(const Bytes& bytes);

// 序列化加入游戏消息。
Bytes game_build_join(const JoinMessage& message);
// 解析加入游戏消息。
JoinMessage game_parse_join(const Bytes& bytes);
// 序列化移动消息。
Bytes game_build_move(const MoveMessage& message);
// 解析移动消息。
MoveMessage game_parse_move(const Bytes& bytes);
// 序列化炮塔朝向消息。
Bytes game_build_target(const TargetMessage& message);
// 解析炮塔朝向消息。
TargetMessage game_parse_target(const Bytes& bytes);
// 序列化单次开火消息。
Bytes game_build_shoot(const ShootMessage& message);
// 解析单次开火消息。
ShootMessage game_parse_shoot(const Bytes& bytes);

// 序列化完整世界快照。
Bytes game_build_state(const BattleStateSnapshot& snapshot);
// 解析完整世界快照。
BattleStateSnapshot game_parse_state(const Bytes& bytes);

// 将世界快照转换为 Web UI 渲染使用的 JSON。
std::string game_format_state_json(const BattleStateSnapshot& snapshot, EntityId self);
} // namespace cyber::game
