#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cyber::game
{
enum class GameMsgType : std::uint8_t
{
    join = 1,
    move = 2,
    target = 3,
    shoot = 4,
    state = 16,
    error = 127
};

enum class PickableType : std::uint8_t
{
    repair = 1,
    damage = 2,
    shield = 3
};

struct GameMessage
{
    GameMsgType type = GameMsgType::error;
    Bytes payload;
};

struct JoinMessage
{
    EntityId client_id = EntityId::unknown;
};

struct MoveMessage
{
    std::int8_t x = 0;
    std::int8_t y = 0;
};

struct TargetMessage
{
    float angle = 0.0F;
};

struct ShootMessage
{};

struct TeamSnapshot
{
    std::uint8_t team_id = 0;
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

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

struct BulletSnapshot
{
    std::uint16_t id = 0;
    EntityId owner_client_id = EntityId::unknown;
    float x = 0.0F;
    float y = 0.0F;
    bool special = false;
};

struct PickableSnapshot
{
    std::uint16_t id = 0;
    PickableType type = PickableType::repair;
    float x = 0.0F;
    float y = 0.0F;
};

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

Bytes game_build_message(const GameMessage& message);
GameMessage game_parse_message(const Bytes& bytes);

Bytes game_build_join(const JoinMessage& message);
JoinMessage game_parse_join(const Bytes& bytes);
Bytes game_build_move(const MoveMessage& message);
MoveMessage game_parse_move(const Bytes& bytes);
Bytes game_build_target(const TargetMessage& message);
TargetMessage game_parse_target(const Bytes& bytes);
Bytes game_build_shoot(const ShootMessage& message);
ShootMessage game_parse_shoot(const Bytes& bytes);

Bytes game_build_state(const BattleStateSnapshot& snapshot);
BattleStateSnapshot game_parse_state(const Bytes& bytes);

std::string game_format_state_json(const BattleStateSnapshot& snapshot, EntityId self);
} // namespace cyber::game
