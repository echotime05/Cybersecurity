#pragma once

#include "cyber/common/types.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstddef>
#include <cstdint>

namespace cyber::game
{
constexpr std::uint64_t kServerTickIntervalMs = 20;
constexpr float kTickScaleFrom33Ms = static_cast<float>(kServerTickIntervalMs) / 33.0F;
constexpr float kTankSpeed = 0.2F * kTickScaleFrom33Ms;
constexpr float kTankRange = 32.0F;
constexpr float kTankRadius = 0.75F;
constexpr float kBulletSpeed = 0.65F * kTickScaleFrom33Ms;
constexpr float kBulletRadius = 0.25F;
constexpr std::int8_t kBulletDamage = 3;
constexpr float kPickableRadius = 0.3F;
constexpr std::uint64_t kRespawnTimeMs = 5000;
constexpr std::uint64_t kInvulnerableTimeMs = 2000;
constexpr std::uint64_t kReloadTimeMs = 400;
constexpr std::uint64_t kRecoveryDelayMs = 3000;
constexpr std::uint64_t kRecoveryIntervalMs = 1000;
constexpr std::uint16_t kWinScore = 10;

struct Vec2
{
    float x = 0.0F;
    float y = 0.0F;
};

struct GameInput
{
    std::int8_t dir_x = 0;
    std::int8_t dir_y = 0;
    float angle = 0.0F;
    bool shoot_requested = false;
};

struct TeamState
{
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

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

struct PickableState
{
    std::uint16_t id = 0;
    PickableType type = PickableType::repair;
    float x = 0.0F;
    float y = 0.0F;
    std::size_t spawn_index = 0;
};

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
