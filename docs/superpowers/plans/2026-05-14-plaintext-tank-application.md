# Plaintext Tank Application Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Phase A of the tank battle application as a plaintext realtime loop: Browser UI <-> local C++ client <-> plaintext TCP <-> authoritative C++ V server.

**Architecture:** Keep the existing `Packet` header and `MsgType::app` transport, but replace the Phase A application payload with a new `GameMessage` protocol modeled after `realtime-tanks-demo`. V owns the authoritative `BattleRoom`; each C++ client owns one TCP connection to V and exposes a local WebSocket for its browser UI.

**Tech Stack:** C++17, CMake, WinSock, existing `cyber_common` packet/socket/logging helpers, small in-repo WebSocket handshake/frame support, TypeScript/Three.js/Vite UI derived from `realtime-tanks-demo/web-threejs`.

---

## Scope Check

This plan covers one coherent milestone: a plaintext playable application layer. It intentionally excludes AS/TGS, DES, RSA signatures, ACK non-repudiation, and replay protection. Those later phases wrap the `GameMessage` payloads built here.

## File Structure

Create focused C++ game modules under `include/cyber/game` and `src/game`:

- `include/cyber/game/game_protocol.hpp`, `src/game/game_protocol.cpp`: binary `GameMessage` and `BattleState` snapshot build/parse helpers.
- `include/cyber/game/game_types.hpp`: game state structs and constants.
- `include/cyber/game/game_world.hpp`, `src/game/game_world.cpp`: `Block`, simple spatial queries, and map data.
- `include/cyber/game/battle_room.hpp`, `src/game/battle_room.cpp`: authoritative game simulation.
- `include/cyber/game/plain_game_server.hpp`, `src/game/plain_game_server.cpp`: `v_server --game-plain` long-connection server.
- `include/cyber/game/plain_game_client.hpp`, `src/game/plain_game_client.cpp`: `client --game-plain --ui-port N` connection controller.

Create local UI bridge support:

- `include/cyber/ui/websocket.hpp`, `src/ui/websocket.cpp`: minimal WebSocket handshake and text-frame support for local browser connections.
- `include/cyber/ui/ui_bridge.hpp`, `src/ui/ui_bridge.cpp`: JSON input/output bridge between browser and C++ client.

Create tests:

- `tests/game_protocol_selftest.cpp`
- `tests/game_world_selftest.cpp`
- `tests/battle_room_selftest.cpp`
- `tests/websocket_selftest.cpp`
- `tests/plain_game_flow_selftest.cpp`

Create Web UI:

- `web-ui/package.json`
- `web-ui/tsconfig.json`
- `web-ui/vite.config.ts`
- `web-ui/index.html`
- `web-ui/src/Network.ts`
- `web-ui/src/Game.ts`
- `web-ui/src/Tank.ts`
- `web-ui/src/MapRenderer.ts`
- `web-ui/src/Sound.ts`
- `web-ui/public/models/*`

Modify:

- `CMakeLists.txt`: add new C++ sources and tests.
- `src/common/role_runtime.cpp`: add `--game-plain` and `--ui-port`.
- `README.md`: document Phase A startup.

---

### Task 1: Game Protocol Roundtrip

**Files:**
- Create: `include/cyber/game/game_protocol.hpp`
- Create: `src/game/game_protocol.cpp`
- Create: `tests/game_protocol_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing protocol self-test**

Create `tests/game_protocol_selftest.cpp`:

```cpp
#include "cyber/game/game_protocol.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

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
        const cyber::game::JoinMessage join{cyber::EntityId::client1, "alpha"};
        const cyber::Bytes join_payload = cyber::game::build_join(join);
        const cyber::game::GameMessage join_msg{
            cyber::game::GameMsgType::join, join_payload};
        const cyber::Bytes encoded_join = cyber::game::build_game_message(join_msg);
        const cyber::game::GameMessage parsed_join =
            cyber::game::parse_game_message(encoded_join);
        require(parsed_join.type == cyber::game::GameMsgType::join, "join type mismatch");
        const cyber::game::JoinMessage parsed_join_body =
            cyber::game::parse_join(parsed_join.payload);
        require(parsed_join_body.client_id == cyber::EntityId::client1, "join client mismatch");
        require(parsed_join_body.name == "alpha", "join name mismatch");

        const cyber::game::MoveMessage move{-1, 1};
        const cyber::game::MoveMessage parsed_move =
            cyber::game::parse_move(cyber::game::build_move(move));
        require(parsed_move.x == -1 && parsed_move.y == 1, "move roundtrip mismatch");

        const cyber::game::TargetMessage target{135.5F};
        const cyber::game::TargetMessage parsed_target =
            cyber::game::parse_target(cyber::game::build_target(target));
        require_close(parsed_target.angle, 135.5F, "target angle mismatch");

        const cyber::game::ShootMessage shoot{true};
        const cyber::game::ShootMessage parsed_shoot =
            cyber::game::parse_shoot(cyber::game::build_shoot(shoot));
        require(parsed_shoot.shooting, "shoot roundtrip mismatch");

        cyber::game::BattleStateSnapshot snapshot;
        snapshot.server_time_ms = 1234;
        snapshot.total_score = 7;
        snapshot.winner_team = -1;
        snapshot.teams.push_back({0, 3, 1});
        snapshot.tanks.push_back({cyber::EntityId::client1, "alpha", 0, 2.5F, 4.5F, 90.0F,
                                  10, 2, false, 3});
        snapshot.bullets.push_back({5, cyber::EntityId::client1, 3.0F, 4.0F, true});
        snapshot.pickables.push_back({9, cyber::game::PickableType::repair, 8.0F, 9.0F});
        const cyber::Bytes snapshot_bytes = cyber::game::build_state(snapshot);
        const cyber::game::BattleStateSnapshot parsed_snapshot =
            cyber::game::parse_state(snapshot_bytes);
        require(parsed_snapshot.server_time_ms == 1234, "state time mismatch");
        require(parsed_snapshot.total_score == 7, "state score mismatch");
        require(parsed_snapshot.winner_team == -1, "state winner mismatch");
        require(parsed_snapshot.teams.size() == 1, "team count mismatch");
        require(parsed_snapshot.tanks.size() == 1, "tank count mismatch");
        require(parsed_snapshot.tanks[0].client_id == cyber::EntityId::client1,
                "tank id mismatch");
        require(parsed_snapshot.tanks[0].name == "alpha", "tank name mismatch");
        require_close(parsed_snapshot.tanks[0].x, 2.5F, "tank x mismatch");
        require(parsed_snapshot.bullets.size() == 1, "bullet count mismatch");
        require(parsed_snapshot.pickables.size() == 1, "pickable count mismatch");

        std::cout << "game_protocol_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "game_protocol_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the failing test target**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
)
```

Add below existing self-test targets:

```cmake
add_executable(game_protocol_selftest tests/game_protocol_selftest.cpp)
target_link_libraries(game_protocol_selftest PRIVATE cyber_common)
```

Add below existing `add_test` calls:

```cmake
add_test(NAME game_protocol_selftest COMMAND game_protocol_selftest)
```

- [ ] **Step 3: Run the failing test**

Run:

```powershell
cmake --build build-mingw --target game_protocol_selftest
.\build-mingw\game_protocol_selftest.exe
```

Expected: build fails because `cyber/game/game_protocol.hpp` does not exist.

- [ ] **Step 4: Create the protocol header**

Create `include/cyber/game/game_protocol.hpp`:

```cpp
#pragma once

#include "cyber/common/packet.hpp"

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
    name = 5,
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
    std::string name;
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
{
    bool shooting = false;
};

struct NameMessage
{
    std::string name;
};

struct TeamSnapshot
{
    std::uint8_t team_id = 0;
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

struct TankSnapshot
{
    EntityId client_id = EntityId::unknown;
    std::string name;
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

Bytes build_game_message(const GameMessage& message);
GameMessage parse_game_message(const Bytes& bytes);

Bytes build_join(const JoinMessage& message);
JoinMessage parse_join(const Bytes& bytes);
Bytes build_move(const MoveMessage& message);
MoveMessage parse_move(const Bytes& bytes);
Bytes build_target(const TargetMessage& message);
TargetMessage parse_target(const Bytes& bytes);
Bytes build_shoot(const ShootMessage& message);
ShootMessage parse_shoot(const Bytes& bytes);
Bytes build_name(const NameMessage& message);
NameMessage parse_name(const Bytes& bytes);

Bytes build_state(const BattleStateSnapshot& snapshot);
BattleStateSnapshot parse_state(const Bytes& bytes);

std::string to_json(const BattleStateSnapshot& snapshot, EntityId self);
} // namespace cyber::game
```

- [ ] **Step 5: Create the protocol implementation**

Create `src/game/game_protocol.cpp` with these implementation details:

```cpp
#include "cyber/game/game_protocol.hpp"

#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cyber::game
{
namespace
{
void write_u16(Bytes& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_u32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_u64(Bytes& out, std::uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
    }
}

std::uint16_t read_u16(const Bytes& in, std::size_t& offset)
{
    if (offset + 2U > in.size())
    {
        throw PacketError("game payload is too short for uint16");
    }
    const auto value = static_cast<std::uint16_t>((in[offset] << 8U) | in[offset + 1U]);
    offset += 2U;
    return value;
}

std::uint32_t read_u32(const Bytes& in, std::size_t& offset)
{
    if (offset + 4U > in.size())
    {
        throw PacketError("game payload is too short for uint32");
    }
    const std::uint32_t value = (static_cast<std::uint32_t>(in[offset]) << 24U) |
                                (static_cast<std::uint32_t>(in[offset + 1U]) << 16U) |
                                (static_cast<std::uint32_t>(in[offset + 2U]) << 8U) |
                                static_cast<std::uint32_t>(in[offset + 3U]);
    offset += 4U;
    return value;
}

std::uint64_t read_u64(const Bytes& in, std::size_t& offset)
{
    if (offset + 8U > in.size())
    {
        throw PacketError("game payload is too short for uint64");
    }
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
    {
        value = (value << 8U) | in[offset + static_cast<std::size_t>(i)];
    }
    offset += 8U;
    return value;
}

void write_f32(Bytes& out, float value)
{
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "float32 size mismatch");
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(out, bits);
}

float read_f32(const Bytes& in, std::size_t& offset)
{
    const std::uint32_t bits = read_u32(in, offset);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_string_u8(Bytes& out, const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint8_t>::max())
    {
        throw PacketError("game string is too long for uint8 length");
    }
    out.push_back(static_cast<std::uint8_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::string read_string_u8(const Bytes& in, std::size_t& offset)
{
    if (offset >= in.size())
    {
        throw PacketError("game string length is missing");
    }
    const std::uint8_t len = in[offset++];
    if (offset + len > in.size())
    {
        throw PacketError("game string exceeds payload");
    }
    std::string out(in.begin() + offset, in.begin() + offset + len);
    offset += len;
    return out;
}

void require_end(const Bytes& in, std::size_t offset, const char* name)
{
    if (offset != in.size())
    {
        throw PacketError(std::string(name) + " has trailing bytes");
    }
}

void write_tank(Bytes& out, const TankSnapshot& tank)
{
    out.push_back(static_cast<std::uint8_t>(tank.client_id));
    write_string_u8(out, tank.name);
    out.push_back(tank.team);
    write_f32(out, tank.x);
    write_f32(out, tank.y);
    write_f32(out, tank.angle);
    out.push_back(static_cast<std::uint8_t>(tank.hp));
    out.push_back(static_cast<std::uint8_t>(tank.shield));
    out.push_back(tank.dead ? 1U : 0U);
    write_u16(out, tank.score);
}

TankSnapshot read_tank(const Bytes& in, std::size_t& offset)
{
    if (offset >= in.size())
    {
        throw PacketError("tank client id missing");
    }
    TankSnapshot tank;
    tank.client_id = static_cast<EntityId>(in[offset++]);
    tank.name = read_string_u8(in, offset);
    if (offset + 1U + 4U + 4U + 4U + 1U + 1U + 1U + 2U > in.size())
    {
        throw PacketError("tank payload is truncated");
    }
    tank.team = in[offset++];
    tank.x = read_f32(in, offset);
    tank.y = read_f32(in, offset);
    tank.angle = read_f32(in, offset);
    tank.hp = static_cast<std::int8_t>(in[offset++]);
    tank.shield = static_cast<std::int8_t>(in[offset++]);
    tank.dead = in[offset++] != 0;
    tank.score = read_u16(in, offset);
    return tank;
}
} // namespace

Bytes build_game_message(const GameMessage& message)
{
    Bytes out;
    out.reserve(1U + message.payload.size());
    out.push_back(static_cast<std::uint8_t>(message.type));
    out.insert(out.end(), message.payload.begin(), message.payload.end());
    return out;
}

GameMessage parse_game_message(const Bytes& bytes)
{
    if (bytes.empty())
    {
        throw PacketError("game message is empty");
    }
    GameMessage message;
    message.type = static_cast<GameMsgType>(bytes[0]);
    message.payload.assign(bytes.begin() + 1, bytes.end());
    return message;
}

Bytes build_join(const JoinMessage& message)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(message.client_id));
    write_string_u8(out, message.name);
    return out;
}

JoinMessage parse_join(const Bytes& bytes)
{
    if (bytes.empty())
    {
        throw PacketError("join payload is empty");
    }
    std::size_t offset = 0;
    JoinMessage message;
    message.client_id = static_cast<EntityId>(bytes[offset++]);
    message.name = read_string_u8(bytes, offset);
    require_end(bytes, offset, "join");
    return message;
}

Bytes build_move(const MoveMessage& message)
{
    return Bytes{static_cast<std::uint8_t>(message.x), static_cast<std::uint8_t>(message.y)};
}

MoveMessage parse_move(const Bytes& bytes)
{
    if (bytes.size() != 2U)
    {
        throw PacketError("move payload must be 2 bytes");
    }
    return {static_cast<std::int8_t>(bytes[0]), static_cast<std::int8_t>(bytes[1])};
}

Bytes build_target(const TargetMessage& message)
{
    Bytes out;
    write_f32(out, message.angle);
    return out;
}

TargetMessage parse_target(const Bytes& bytes)
{
    std::size_t offset = 0;
    TargetMessage message{read_f32(bytes, offset)};
    require_end(bytes, offset, "target");
    return message;
}

Bytes build_shoot(const ShootMessage& message)
{
    return Bytes{static_cast<std::uint8_t>(message.shooting ? 1U : 0U)};
}

ShootMessage parse_shoot(const Bytes& bytes)
{
    if (bytes.size() != 1U)
    {
        throw PacketError("shoot payload must be 1 byte");
    }
    return {bytes[0] != 0};
}

Bytes build_name(const NameMessage& message)
{
    Bytes out;
    write_string_u8(out, message.name);
    return out;
}

NameMessage parse_name(const Bytes& bytes)
{
    std::size_t offset = 0;
    NameMessage message{read_string_u8(bytes, offset)};
    require_end(bytes, offset, "name");
    return message;
}

Bytes build_state(const BattleStateSnapshot& snapshot)
{
    Bytes out;
    write_u64(out, snapshot.server_time_ms);
    write_u16(out, snapshot.total_score);
    out.push_back(static_cast<std::uint8_t>(snapshot.winner_team));
    if (snapshot.teams.size() > 255U || snapshot.tanks.size() > 255U)
    {
        throw PacketError("too many teams or tanks for state payload");
    }
    if (snapshot.bullets.size() > 65535U || snapshot.pickables.size() > 65535U)
    {
        throw PacketError("too many bullets or pickables for state payload");
    }
    out.push_back(static_cast<std::uint8_t>(snapshot.teams.size()));
    for (const TeamSnapshot& team : snapshot.teams)
    {
        out.push_back(team.team_id);
        write_u16(out, team.score);
        out.push_back(team.tanks);
    }
    out.push_back(static_cast<std::uint8_t>(snapshot.tanks.size()));
    for (const TankSnapshot& tank : snapshot.tanks)
    {
        write_tank(out, tank);
    }
    write_u16(out, static_cast<std::uint16_t>(snapshot.bullets.size()));
    for (const BulletSnapshot& bullet : snapshot.bullets)
    {
        write_u16(out, bullet.id);
        out.push_back(static_cast<std::uint8_t>(bullet.owner_client_id));
        write_f32(out, bullet.x);
        write_f32(out, bullet.y);
        out.push_back(bullet.special ? 1U : 0U);
    }
    write_u16(out, static_cast<std::uint16_t>(snapshot.pickables.size()));
    for (const PickableSnapshot& pickable : snapshot.pickables)
    {
        write_u16(out, pickable.id);
        out.push_back(static_cast<std::uint8_t>(pickable.type));
        write_f32(out, pickable.x);
        write_f32(out, pickable.y);
    }
    return out;
}

BattleStateSnapshot parse_state(const Bytes& bytes)
{
    std::size_t offset = 0;
    BattleStateSnapshot snapshot;
    snapshot.server_time_ms = read_u64(bytes, offset);
    snapshot.total_score = read_u16(bytes, offset);
    if (offset >= bytes.size())
    {
        throw PacketError("state winner team missing");
    }
    snapshot.winner_team = static_cast<std::int8_t>(bytes[offset++]);
    if (offset >= bytes.size())
    {
        throw PacketError("state team count missing");
    }
    const std::uint8_t team_count = bytes[offset++];
    for (std::uint8_t i = 0; i < team_count; ++i)
    {
        if (offset + 4U > bytes.size())
        {
            throw PacketError("team payload is truncated");
        }
        TeamSnapshot team;
        team.team_id = bytes[offset++];
        team.score = read_u16(bytes, offset);
        team.tanks = bytes[offset++];
        snapshot.teams.push_back(team);
    }
    if (offset >= bytes.size())
    {
        throw PacketError("state tank count missing");
    }
    const std::uint8_t tank_count = bytes[offset++];
    for (std::uint8_t i = 0; i < tank_count; ++i)
    {
        snapshot.tanks.push_back(read_tank(bytes, offset));
    }
    const std::uint16_t bullet_count = read_u16(bytes, offset);
    for (std::uint16_t i = 0; i < bullet_count; ++i)
    {
        BulletSnapshot bullet;
        bullet.id = read_u16(bytes, offset);
        if (offset + 1U > bytes.size())
        {
            throw PacketError("bullet owner missing");
        }
        bullet.owner_client_id = static_cast<EntityId>(bytes[offset++]);
        bullet.x = read_f32(bytes, offset);
        bullet.y = read_f32(bytes, offset);
        if (offset >= bytes.size())
        {
            throw PacketError("bullet special missing");
        }
        bullet.special = bytes[offset++] != 0;
        snapshot.bullets.push_back(bullet);
    }
    const std::uint16_t pickable_count = read_u16(bytes, offset);
    for (std::uint16_t i = 0; i < pickable_count; ++i)
    {
        PickableSnapshot pickable;
        pickable.id = read_u16(bytes, offset);
        if (offset >= bytes.size())
        {
            throw PacketError("pickable type missing");
        }
        pickable.type = static_cast<PickableType>(bytes[offset++]);
        pickable.x = read_f32(bytes, offset);
        pickable.y = read_f32(bytes, offset);
        snapshot.pickables.push_back(pickable);
    }
    require_end(bytes, offset, "state");
    return snapshot;
}

std::string to_json(const BattleStateSnapshot& snapshot, EntityId self)
{
    std::ostringstream out;
    out << "{\"type\":\"state\",\"self\":" << static_cast<int>(self)
        << ",\"state\":{\"serverTimeMs\":" << snapshot.server_time_ms
        << ",\"totalScore\":" << snapshot.total_score
        << ",\"winnerTeam\":" << static_cast<int>(snapshot.winner_team)
        << ",\"teams\":[";
    for (std::size_t i = 0; i < snapshot.teams.size(); ++i)
    {
        const TeamSnapshot& team = snapshot.teams[i];
        if (i != 0) out << ',';
        out << "{\"teamId\":" << static_cast<int>(team.team_id)
            << ",\"score\":" << team.score
            << ",\"tanks\":" << static_cast<int>(team.tanks) << '}';
    }
    out << "],\"tanks\":[";
    for (std::size_t i = 0; i < snapshot.tanks.size(); ++i)
    {
        const TankSnapshot& tank = snapshot.tanks[i];
        if (i != 0) out << ',';
        out << "{\"clientId\":" << static_cast<int>(tank.client_id)
            << ",\"name\":\"" << tank.name
            << "\",\"team\":" << static_cast<int>(tank.team)
            << ",\"x\":" << tank.x
            << ",\"y\":" << tank.y
            << ",\"angle\":" << tank.angle
            << ",\"hp\":" << static_cast<int>(tank.hp)
            << ",\"shield\":" << static_cast<int>(tank.shield)
            << ",\"dead\":" << (tank.dead ? "true" : "false")
            << ",\"score\":" << tank.score << '}';
    }
    out << "],\"bullets\":[";
    for (std::size_t i = 0; i < snapshot.bullets.size(); ++i)
    {
        const BulletSnapshot& bullet = snapshot.bullets[i];
        if (i != 0) out << ',';
        out << "{\"id\":" << bullet.id
            << ",\"ownerClientId\":" << static_cast<int>(bullet.owner_client_id)
            << ",\"x\":" << bullet.x
            << ",\"y\":" << bullet.y
            << ",\"special\":" << (bullet.special ? "true" : "false") << '}';
    }
    out << "],\"pickables\":[";
    for (std::size_t i = 0; i < snapshot.pickables.size(); ++i)
    {
        const PickableSnapshot& pickable = snapshot.pickables[i];
        if (i != 0) out << ',';
        out << "{\"id\":" << pickable.id
            << ",\"type\":" << static_cast<int>(pickable.type)
            << ",\"x\":" << pickable.x
            << ",\"y\":" << pickable.y << '}';
    }
    out << "]}}";
    return out.str();
}
} // namespace cyber::game
```

- [ ] **Step 6: Run the protocol test**

Run:

```powershell
cmake --build build-mingw --target game_protocol_selftest
.\build-mingw\game_protocol_selftest.exe
```

Expected: output contains `game_protocol_selftest: ok`.

- [ ] **Step 7: Commit protocol module**

Run:

```powershell
git add CMakeLists.txt include/cyber/game/game_protocol.hpp src/game/game_protocol.cpp tests/game_protocol_selftest.cpp
git commit -m "feat: add plaintext game protocol"
```

---

### Task 2: Game Types and World Primitives

**Files:**
- Create: `include/cyber/game/game_types.hpp`
- Create: `include/cyber/game/game_world.hpp`
- Create: `src/game/game_world.cpp`
- Create: `tests/game_world_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing world self-test**

Create `tests/game_world_selftest.cpp`:

```cpp
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
        world.for_each_around(cyber::game::SpatialLayer::tank, a, [&](cyber::game::SpatialItem& item) {
            if (&item == &b)
            {
                ++seen;
            }
        });
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
```

- [ ] **Step 2: Register the failing world test**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
    src/game/game_world.cpp
)
```

Add:

```cmake
add_executable(game_world_selftest tests/game_world_selftest.cpp)
target_link_libraries(game_world_selftest PRIVATE cyber_common)
add_test(NAME game_world_selftest COMMAND game_world_selftest)
```

- [ ] **Step 3: Run the failing world test**

Run:

```powershell
cmake --build build-mingw --target game_world_selftest
.\build-mingw\game_world_selftest.exe
```

Expected: build fails because `cyber/game/game_world.hpp` does not exist.

- [ ] **Step 4: Create game state types**

Create `include/cyber/game/game_types.hpp`:

```cpp
#pragma once

#include "cyber/common/types.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstdint>
#include <string>

namespace cyber::game
{
constexpr float kTankSpeed = 0.3F;
constexpr float kTankRange = 16.0F;
constexpr float kTankRadius = 0.75F;
constexpr float kBulletSpeed = 0.7F;
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
    bool shooting = false;
};

struct TeamState
{
    std::uint16_t score = 0;
    std::uint8_t tanks = 0;
};

struct TankState
{
    EntityId client_id = EntityId::unknown;
    std::string name = "guest";
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
```

- [ ] **Step 5: Create world header**

Create `include/cyber/game/game_world.hpp`:

```cpp
#pragma once

#include "cyber/game/game_types.hpp"

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace cyber::game
{
struct SpatialItem
{
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.0F;
    int node_index = -1;
};

enum class SpatialLayer : std::uint8_t
{
    tank = 0,
    bullet = 1,
    pickable = 2,
    block = 3
};

class Block : public SpatialItem
{
public:
    Block(float center_x, float center_y, float width, float height);
    std::optional<Vec2> collide_circle(float cx, float cy, float radius) const;

    float width() const;
    float height() const;

private:
    float width_ = 0.0F;
    float height_ = 0.0F;
};

class World
{
public:
    World(float width, float height, float cluster_size);

    void add(SpatialLayer layer, SpatialItem* item);
    void remove(SpatialLayer layer, SpatialItem& item);
    void update(SpatialLayer layer, SpatialItem& item);
    void for_each_around(SpatialLayer layer, const SpatialItem& item,
                         const std::function<void(SpatialItem&)>& fn,
                         const SpatialItem* exclude = nullptr);

    float width() const;
    float height() const;

private:
    struct Node
    {
        std::vector<SpatialItem*> items;
    };

    struct LayerData
    {
        std::vector<Node> nodes;
    };

    int pick(float x, float y) const;
    LayerData& layer(SpatialLayer layer);

    float width_ = 0.0F;
    float height_ = 0.0F;
    float cluster_size_ = 1.0F;
    int cluster_width_ = 1;
    int cluster_height_ = 1;
    std::array<LayerData, 4> layers_;
};

const std::vector<Block>& level_blocks();
const std::vector<PickableSpawn>& pickable_spawns();
} // namespace cyber::game
```

- [ ] **Step 6: Implement world primitives**

Create `src/game/game_world.cpp` with block collision, spatial clustering, and the demo map constants:

```cpp
#include "cyber/game/game_world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cyber::game
{
Block::Block(float center_x, float center_y, float width, float height)
{
    x = center_x;
    y = center_y;
    radius = std::max(width, height) * 0.5F;
    width_ = width;
    height_ = height;
}

std::optional<Vec2> Block::collide_circle(float cx, float cy, float circle_radius) const
{
    const float left = x - width_ * 0.5F;
    const float right = x + width_ * 0.5F;
    const float top = y - height_ * 0.5F;
    const float bottom = y + height_ * 0.5F;
    const float nearest_x = std::max(left, std::min(cx, right));
    const float nearest_y = std::max(top, std::min(cy, bottom));
    const float dx = cx - nearest_x;
    const float dy = cy - nearest_y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 >= circle_radius * circle_radius)
    {
        return std::nullopt;
    }
    if (dist2 > 0.0001F)
    {
        const float dist = std::sqrt(dist2);
        const float push = circle_radius - dist;
        return Vec2{(dx / dist) * push, (dy / dist) * push};
    }
    const float push_left = std::abs(cx - left);
    const float push_right = std::abs(right - cx);
    const float push_top = std::abs(cy - top);
    const float push_bottom = std::abs(bottom - cy);
    const float best = std::min(std::min(push_left, push_right), std::min(push_top, push_bottom));
    if (best == push_left)
    {
        return Vec2{-(circle_radius + push_left), 0.0F};
    }
    if (best == push_right)
    {
        return Vec2{circle_radius + push_right, 0.0F};
    }
    if (best == push_top)
    {
        return Vec2{0.0F, -(circle_radius + push_top)};
    }
    return Vec2{0.0F, circle_radius + push_bottom};
}

float Block::width() const { return width_; }
float Block::height() const { return height_; }

World::World(float width, float height, float cluster_size)
    : width_(width), height_(height), cluster_size_(cluster_size)
{
    cluster_width_ = std::max(1, static_cast<int>(width_ / cluster_size_));
    cluster_height_ = std::max(1, static_cast<int>(height_ / cluster_size_));
    for (LayerData& layer_data : layers_)
    {
        layer_data.nodes.resize(static_cast<std::size_t>(cluster_width_ * cluster_height_));
    }
}

int World::pick(float px, float py) const
{
    const int ix = std::max(0, std::min(cluster_width_ - 1, static_cast<int>(px / cluster_size_)));
    const int iy = std::max(0, std::min(cluster_height_ - 1, static_cast<int>(py / cluster_size_)));
    return iy * cluster_width_ + ix;
}

World::LayerData& World::layer(SpatialLayer layer_name)
{
    return layers_[static_cast<std::size_t>(layer_name)];
}

void World::add(SpatialLayer layer_name, SpatialItem* item)
{
    if (item == nullptr)
    {
        throw std::runtime_error("cannot add null spatial item");
    }
    const int index = pick(item->x, item->y);
    item->node_index = index;
    layer(layer_name).nodes[static_cast<std::size_t>(index)].items.push_back(item);
}

void World::remove(SpatialLayer layer_name, SpatialItem& item)
{
    if (item.node_index < 0)
    {
        return;
    }
    std::vector<SpatialItem*>& items =
        layer(layer_name).nodes[static_cast<std::size_t>(item.node_index)].items;
    items.erase(std::remove(items.begin(), items.end(), &item), items.end());
    item.node_index = -1;
}

void World::update(SpatialLayer layer_name, SpatialItem& item)
{
    const float r = item.radius;
    item.x = std::max(r, std::min(width_ - r, item.x));
    item.y = std::max(r, std::min(height_ - r, item.y));
    const int new_index = pick(item.x, item.y);
    if (new_index == item.node_index)
    {
        return;
    }
    remove(layer_name, item);
    item.node_index = new_index;
    layer(layer_name).nodes[static_cast<std::size_t>(new_index)].items.push_back(&item);
}

void World::for_each_around(SpatialLayer layer_name, const SpatialItem& item,
                            const std::function<void(SpatialItem&)>& fn,
                            const SpatialItem* exclude)
{
    const int center = pick(item.x, item.y);
    const int cx = center % cluster_width_;
    const int cy = center / cluster_width_;
    for (int ny = std::max(0, cy - 1); ny <= std::min(cluster_height_ - 1, cy + 1); ++ny)
    {
        for (int nx = std::max(0, cx - 1); nx <= std::min(cluster_width_ - 1, cx + 1); ++nx)
        {
            std::vector<SpatialItem*>& items =
                layer(layer_name).nodes[static_cast<std::size_t>(ny * cluster_width_ + nx)].items;
            for (SpatialItem* other : items)
            {
                if (other != nullptr && other != exclude)
                {
                    fn(*other);
                }
            }
        }
    }
}

float World::width() const { return width_; }
float World::height() const { return height_; }

const std::vector<Block>& level_blocks()
{
    static const std::vector<Block> blocks = {
        {13.5F, 2.0F, 1.0F, 4.0F},    {13.5F, 12.0F, 1.0F, 2.0F},
        {12.5F, 13.5F, 3.0F, 1.0F},   {2.0F, 13.5F, 4.0F, 1.0F},
        {11.5F, 15.0F, 1.0F, 2.0F},   {11.5F, 23.5F, 1.0F, 5.0F},
        {10.0F, 26.5F, 4.0F, 1.0F},   {6.0F, 26.5F, 4.0F, 1.0F},
        {2.0F, 34.5F, 4.0F, 1.0F},    {12.5F, 34.5F, 3.0F, 1.0F},
        {13.5F, 36.0F, 1.0F, 2.0F},   {15.0F, 36.5F, 2.0F, 1.0F},
        {13.5F, 46.0F, 1.0F, 4.0F},   {23.5F, 36.5F, 5.0F, 1.0F},
        {26.5F, 38.0F, 1.0F, 4.0F},   {26.5F, 42.0F, 1.0F, 4.0F},
        {34.5F, 46.0F, 1.0F, 4.0F},   {34.5F, 36.0F, 1.0F, 2.0F},
        {35.5F, 34.5F, 3.0F, 1.0F},   {36.5F, 33.0F, 1.0F, 2.0F},
        {46.0F, 34.5F, 4.0F, 1.0F},   {36.5F, 24.5F, 1.0F, 5.0F},
        {38.0F, 21.5F, 4.0F, 1.0F},   {42.0F, 21.5F, 4.0F, 1.0F},
        {46.0F, 13.5F, 4.0F, 1.0F},   {35.5F, 13.5F, 3.0F, 1.0F},
        {34.5F, 12.0F, 1.0F, 2.0F},   {33.0F, 11.5F, 2.0F, 1.0F},
        {34.5F, 2.0F, 1.0F, 4.0F},    {24.5F, 11.5F, 5.0F, 1.0F},
        {21.5F, 10.0F, 1.0F, 4.0F},   {21.5F, 6.0F, 1.0F, 4.0F},
        {18.5F, 22.0F, 1.0F, 6.0F},   {19.0F, 18.5F, 2.0F, 1.0F},
        {26.0F, 18.5F, 6.0F, 1.0F},   {29.5F, 19.0F, 1.0F, 2.0F},
        {29.5F, 26.0F, 1.0F, 6.0F},   {29.0F, 29.5F, 2.0F, 1.0F},
        {22.0F, 29.5F, 6.0F, 1.0F},   {18.5F, 29.0F, 1.0F, 2.0F}};
    return blocks;
}

const std::vector<PickableSpawn>& pickable_spawns()
{
    static const std::vector<PickableSpawn> spawns = {
        {23.5F, 9.5F, PickableType::repair, 5000, 0, 0},
        {38.5F, 23.5F, PickableType::repair, 5000, 0, 0},
        {24.5F, 38.5F, PickableType::repair, 5000, 0, 0},
        {9.5F, 24.5F, PickableType::repair, 5000, 0, 0},
        {13.5F, 15.5F, PickableType::damage, 10000, 0, 0},
        {32.5F, 13.5F, PickableType::damage, 10000, 0, 0},
        {34.5F, 32.5F, PickableType::damage, 10000, 0, 0},
        {15.5F, 34.5F, PickableType::damage, 10000, 0, 0},
        {24.0F, 24.0F, PickableType::shield, 30000, 0, 0}};
    return spawns;
}
} // namespace cyber::game
```

- [ ] **Step 7: Run the world test**

Run:

```powershell
cmake --build build-mingw --target game_world_selftest
.\build-mingw\game_world_selftest.exe
```

Expected: output contains `game_world_selftest: ok`.

- [ ] **Step 8: Commit world primitives**

Run:

```powershell
git add CMakeLists.txt include/cyber/game/game_types.hpp include/cyber/game/game_world.hpp src/game/game_world.cpp tests/game_world_selftest.cpp
git commit -m "feat: add tank game world primitives"
```

---

### Task 3: BattleRoom Simulation

**Files:**
- Create: `include/cyber/game/battle_room.hpp`
- Create: `src/game/battle_room.cpp`
- Create: `tests/battle_room_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing BattleRoom self-test**

Create `tests/battle_room_selftest.cpp`:

```cpp
#include "cyber/game/battle_room.hpp"

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
} // namespace

int main()
{
    try
    {
        cyber::game::BattleRoom room;
        room.join(cyber::EntityId::client1, "alpha", 1000);
        room.join(cyber::EntityId::client2, "bravo", 1000);
        auto state = room.snapshot(1000);
        require(state.tanks.size() == 2U, "join did not create two tanks");
        require(state.teams.size() == 4U, "team count mismatch");
        require(state.tanks[0].team != state.tanks[1].team, "first two tanks should split teams");

        room.handle_move(cyber::EntityId::client1, 1, 0);
        const float before_x = room.snapshot(1000).tanks[0].x;
        room.tick(5050);
        const float after_x = room.snapshot(5050).tanks[0].x;
        require(after_x > before_x, "move did not advance tank");

        room.handle_target(cyber::EntityId::client1, 90.0F);
        room.handle_shoot(cyber::EntityId::client1, true);
        room.tick(5500);
        state = room.snapshot(5500);
        require(!state.bullets.empty(), "shoot did not create bullet");

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
```

- [ ] **Step 2: Register the failing BattleRoom test**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
)
```

Add:

```cmake
add_executable(battle_room_selftest tests/battle_room_selftest.cpp)
target_link_libraries(battle_room_selftest PRIVATE cyber_common)
add_test(NAME battle_room_selftest COMMAND battle_room_selftest)
```

- [ ] **Step 3: Run the failing BattleRoom test**

Run:

```powershell
cmake --build build-mingw --target battle_room_selftest
.\build-mingw\battle_room_selftest.exe
```

Expected: build fails because `cyber/game/battle_room.hpp` does not exist.

- [ ] **Step 4: Create BattleRoom header**

Create `include/cyber/game/battle_room.hpp`:

```cpp
#pragma once

#include "cyber/game/game_protocol.hpp"
#include "cyber/game/game_types.hpp"
#include "cyber/game/game_world.hpp"

#include <array>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace cyber::game
{
class BattleRoom
{
public:
    BattleRoom();

    bool join(EntityId client_id, const std::string& name, std::uint64_t now_ms);
    void leave(EntityId client_id);
    void handle_move(EntityId client_id, std::int8_t x, std::int8_t y);
    void handle_target(EntityId client_id, float angle);
    void handle_shoot(EntityId client_id, bool shooting);
    void handle_name(EntityId client_id, const std::string& name);
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
    bool valid_name(const std::string& name) const;
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
```

- [ ] **Step 5: Implement BattleRoom**

Create `src/game/battle_room.cpp`. Port the demo behavior directly. Keep these invariants in the code:

```cpp
#include "cyber/game/battle_room.hpp"

#include <algorithm>
#include <cmath>
#include <regex>

namespace cyber::game
{
namespace
{
float length(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

int client_index(EntityId id)
{
    return static_cast<int>(id) - static_cast<int>(EntityId::client1);
}
} // namespace

BattleRoom::BattleRoom() : world_(48.0F, 48.0F, 4.0F)
{
    blocks_ = level_blocks();
    pick_spawns_ = pickable_spawns();
    rebuild_world();
}

bool BattleRoom::valid_name(const std::string& name) const
{
    static const std::regex pattern("^[A-Za-z0-9_-]{4,8}$");
    return std::regex_match(name, pattern);
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
    tank.x = 2.5F + static_cast<float>(tank.team % 2U) * 35.0F + static_cast<float>((index * 3) % 9);
    tank.y = 2.5F + static_cast<float>(tank.team / 2U) * 35.0F + static_cast<float>((index * 5) % 9);
}

bool BattleRoom::join(EntityId client_id, const std::string& name, std::uint64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_client(client_id) || tanks_.count(client_id) != 0U)
    {
        return false;
    }
    const std::uint8_t team = pick_weakest_team();
    TankItem item;
    item.state.client_id = client_id;
    item.state.name = valid_name(name) ? name : "guest";
    item.state.team = team;
    item.state.dead = true;
    item.state.died_ms = now_ms;
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
    it->second.state.input.dir_x = static_cast<std::int8_t>(std::max(-1, std::min(1, static_cast<int>(x))));
    it->second.state.input.dir_y = static_cast<std::int8_t>(std::max(-1, std::min(1, static_cast<int>(y))));
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

void BattleRoom::handle_shoot(EntityId client_id, bool shooting)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it != tanks_.end())
    {
        it->second.state.input.shooting = shooting;
    }
}

void BattleRoom::handle_name(EntityId client_id, const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tanks_.find(client_id);
    if (it != tanks_.end() && valid_name(name))
    {
        it->second.state.name = name;
    }
}
```

Add the remaining `BattleRoom` methods using these acceptance rules:

- In `tick()`, if a dead tank has been dead for more than `kRespawnTimeMs`, respawn it with `hp=10`, `shield=0`, `ammo=0`, `dead=false`, and a new spawn position.
- For live tanks, apply normalized movement using `kTankSpeed`.
- Use `Block::collide_circle()` for block collision pushback.
- On tank/tank overlap, split the overlap between both tanks.
- Spawn pickables when `active_id == 0` and `now_ms - picked_ms > delay_ms`.
- On tank/pickable collision, apply repair, damage ammo, or shield.
- On shooting and not reloading, call `create_bullet()`.
- Move each bullet toward its target; remove it when it reaches target, leaves world, hits a block, or hits an enemy tank.
- On bullet hit, apply shield first, then HP. If HP reaches zero, increment owner tank score, owner team score, `total_score_`, mark victim dead, set `died_ms`, and stop victim shooting.
- If team score reaches `kWinScore`, set `winner_team_`, reset team scores and tank scores, set all live tanks dead, and clear `winner_team_` after 3000 ms.

- [ ] **Step 6: Run the BattleRoom test**

Run:

```powershell
cmake --build build-mingw --target battle_room_selftest
.\build-mingw\battle_room_selftest.exe
```

Expected: output contains `battle_room_selftest: ok`.

- [ ] **Step 7: Commit BattleRoom**

Run:

```powershell
git add CMakeLists.txt include/cyber/game/battle_room.hpp src/game/battle_room.cpp tests/battle_room_selftest.cpp
git commit -m "feat: add authoritative battle room"
```

---

### Task 4: Plaintext V Game Server

**Files:**
- Create: `include/cyber/game/plain_game_server.hpp`
- Create: `src/game/plain_game_server.cpp`
- Create: `tests/plain_game_flow_selftest.cpp`
- Modify: `src/common/role_runtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing plaintext flow self-test**

Create `tests/plain_game_flow_selftest.cpp`:

```cpp
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/game/plain_game_server.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        cyber::SocketRuntime runtime;
        cyber::game::PlainGameServer server({"127.0.0.1", 0});
        const std::uint16_t port = server.start_for_test();
        std::thread server_thread([&]() { server.run_until_stopped(); });

        cyber::Logger client_logger(std::filesystem::temp_directory_path() / "plain_game_flow_client.log");
        cyber::SocketHandle client = cyber::connect_tcp({"127.0.0.1", port});
        const auto join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "alpha"})});
        cyber::send_packet_logged(client,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1,
                                                     cyber::EntityId::v, join),
                                  client_logger, "Client", "PlainGameTest");
        bool saw_state = false;
        for (int i = 0; i < 10 && !saw_state; ++i)
        {
            const cyber::Packet packet =
                cyber::recv_packet_logged(client, client_logger, "Client", "PlainGameTest");
            const cyber::game::GameMessage message =
                cyber::game::parse_game_message(packet.payload);
            if (message.type == cyber::game::GameMsgType::state)
            {
                const cyber::game::BattleStateSnapshot state =
                    cyber::game::parse_state(message.payload);
                saw_state = !state.tanks.empty();
            }
        }
        require(saw_state, "did not receive state with joined tank");

        cyber::close_socket(client);
        server.stop();
        server_thread.join();
        std::cout << "plain_game_flow_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "plain_game_flow_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the failing plaintext server test**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
    src/game/plain_game_server.cpp
)
```

Add:

```cmake
add_executable(plain_game_flow_selftest tests/plain_game_flow_selftest.cpp)
target_link_libraries(plain_game_flow_selftest PRIVATE cyber_common)
add_test(NAME plain_game_flow_selftest COMMAND plain_game_flow_selftest)
```

- [ ] **Step 3: Run the failing plaintext flow test**

Run:

```powershell
cmake --build build-mingw --target plain_game_flow_selftest
.\build-mingw\plain_game_flow_selftest.exe
```

Expected: build fails because `plain_game_server.hpp` does not exist.

- [ ] **Step 4: Create plaintext server header**

Create `include/cyber/game/plain_game_server.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/battle_room.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace cyber::game
{
class PlainGameServer
{
public:
    explicit PlainGameServer(TcpEndpoint endpoint);
    ~PlainGameServer();

    void run();
    std::uint16_t start_for_test();
    void run_until_stopped();
    void stop();

private:
    struct ClientConnection
    {
        SocketHandle socket = 0;
        EntityId client_id = EntityId::unknown;
    };

    void accept_loop();
    void client_loop(SocketHandle socket, std::string peer);
    void game_loop();
    void broadcast(const BattleStateSnapshot& snapshot);
    void handle_packet(SocketHandle socket, const Packet& packet);

    TcpEndpoint endpoint_;
    SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex connections_mutex_;
    std::map<EntityId, ClientConnection> connections_;
    BattleRoom room_;
    Logger logger_;
};
} // namespace cyber::game
```

- [ ] **Step 5: Implement plaintext server**

Create `src/game/plain_game_server.cpp` with these behaviors:

```cpp
#include "cyber/game/plain_game_server.hpp"

#include "cyber/common/net_packet.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace cyber::game
{
PlainGameServer::PlainGameServer(TcpEndpoint endpoint)
    : endpoint_(std::move(endpoint)), logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}

PlainGameServer::~PlainGameServer()
{
    stop();
}

void PlainGameServer::run()
{
    listener_ = listen_tcp(endpoint_);
    std::cout << "V plaintext game listening on " << endpoint_.ip << ':' << endpoint_.port << '\n';
    run_until_stopped();
}

std::uint16_t PlainGameServer::start_for_test()
{
    listener_ = listen_tcp(endpoint_);
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getsockname(static_cast<SOCKET>(listener_), reinterpret_cast<sockaddr*>(&addr), &len) != 0)
    {
        throw std::runtime_error("getsockname failed for plaintext game server");
    }
    endpoint_.port = ntohs(addr.sin_port);
    return endpoint_.port;
}

void PlainGameServer::run_until_stopped()
{
    std::thread game_thread([&]() { game_loop(); });
    try
    {
        accept_loop();
    }
    catch (...)
    {
        stopping_ = true;
        if (listener_ != 0)
        {
            close_socket(listener_);
            listener_ = 0;
        }
        game_thread.join();
        throw;
    }
    stopping_ = true;
    game_thread.join();
}

void PlainGameServer::stop()
{
    stopping_ = true;
    if (listener_ != 0)
    {
        close_socket(listener_);
        listener_ = 0;
    }
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, connection] : connections_)
    {
        if (connection.socket != 0)
        {
            close_socket(connection.socket);
            connection.socket = 0;
        }
    }
}
```

Add the remaining `PlainGameServer` methods using these acceptance rules:

- `accept_loop()` accepts sockets and detaches `client_loop()` threads while `!stopping_`.
- `client_loop()` reads `Packet` values with `recv_packet_logged()`, calls `handle_packet()`, and removes the client from `connections_` and `BattleRoom` on disconnect.
- `handle_packet()` rejects non-`MsgType::app`, parses `GameMessage`, and dispatches `join/move/target/shoot/name`.
- On `join`, call `room_.join()` and store the `EntityId -> socket` mapping.
- `game_loop()` sleeps until 50 ms intervals, calls `room_.tick(now_ms)`, snapshots the room, and calls `broadcast()`.
- `broadcast()` serializes one `GameMsgType::state` payload and sends the same `Packet` to every client in a copied connection list.

Use `std::chrono::steady_clock` for tick timing and `std::chrono::system_clock` milliseconds for serialized `server_time_ms`.

- [ ] **Step 6: Add role runtime CLI dispatch**

Modify `src/common/role_runtime.cpp` includes:

```cpp
#include "cyber/game/plain_game_server.hpp"
```

Add parse flag state in `run_role_main()`:

```cpp
bool game_plain = false;
std::uint16_t ui_port = 0;
```

Add argument parsing:

```cpp
else if (arg == "--game-plain")
{
    game_plain = true;
}
else if (arg == "--ui-port")
{
    if (i + 1 >= argc)
    {
        std::cerr << "--ui-port requires a port\n";
        return 2;
    }
    ui_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
}
```

Before `self_test` handling, add:

```cpp
if (game_plain)
{
    if (role == RoleKind::v_server)
    {
        SocketRuntime runtime;
        cyber::game::PlainGameServer server(bind_endpoint(config, spec));
        server.run();
        return 0;
    }
    if (role == RoleKind::client)
    {
        std::cout << "client --game-plain will be enabled after PlainGameClient is added\n";
        return 2;
    }
    throw std::runtime_error("--game-plain is only supported by v_server and client");
}
```

- [ ] **Step 7: Run plaintext server test**

Run:

```powershell
cmake --build build-mingw --target plain_game_flow_selftest
.\build-mingw\plain_game_flow_selftest.exe
```

Expected: output contains `plain_game_flow_selftest: ok`.

- [ ] **Step 8: Commit plaintext V server**

Run:

```powershell
git add CMakeLists.txt include/cyber/game/plain_game_server.hpp src/game/plain_game_server.cpp src/common/role_runtime.cpp tests/plain_game_flow_selftest.cpp
git commit -m "feat: add plaintext V game server"
```

---

### Task 5: Local WebSocket Support

**Files:**
- Create: `include/cyber/ui/websocket.hpp`
- Create: `src/ui/websocket.cpp`
- Create: `tests/websocket_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing WebSocket self-test**

Create `tests/websocket_selftest.cpp`:

```cpp
#include "cyber/ui/websocket.hpp"

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
} // namespace

int main()
{
    try
    {
        const std::string accept =
            cyber::ui::websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==");
        require(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC accept key mismatch");

        const cyber::Bytes server_frame = cyber::ui::build_ws_text_frame("hello");
        require(server_frame.size() == 7U, "server frame size mismatch");
        require(server_frame[0] == 0x81 && server_frame[1] == 0x05, "server frame header mismatch");

        const cyber::Bytes masked_client_frame{
            0x81, 0x85, 0x37, 0xFA, 0x21, 0x3D,
            static_cast<std::uint8_t>('h' ^ 0x37),
            static_cast<std::uint8_t>('e' ^ 0xFA),
            static_cast<std::uint8_t>('l' ^ 0x21),
            static_cast<std::uint8_t>('l' ^ 0x3D),
            static_cast<std::uint8_t>('o' ^ 0x37)};
        const cyber::ui::WebSocketFrame parsed =
            cyber::ui::parse_ws_frame(masked_client_frame);
        require(parsed.opcode == 1, "parsed opcode mismatch");
        require(parsed.text == "hello", "parsed text mismatch");

        std::cout << "websocket_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "websocket_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the failing WebSocket test**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
    src/game/plain_game_server.cpp
    src/ui/websocket.cpp
)
```

Update the existing Windows library link line:

```cmake
target_link_libraries(cyber_common PUBLIC ws2_32 advapi32)
```

`advapi32` is required for the Windows CryptoAPI SHA-1 helper used by the WebSocket handshake.

Add:

```cmake
add_executable(websocket_selftest tests/websocket_selftest.cpp)
target_link_libraries(websocket_selftest PRIVATE cyber_common)
add_test(NAME websocket_selftest COMMAND websocket_selftest)
```

- [ ] **Step 3: Run the failing WebSocket test**

Run:

```powershell
cmake --build build-mingw --target websocket_selftest
.\build-mingw\websocket_selftest.exe
```

Expected: build fails because `cyber/ui/websocket.hpp` does not exist.

- [ ] **Step 4: Create WebSocket header**

Create `include/cyber/ui/websocket.hpp`:

```cpp
#pragma once

#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"

#include <string>

namespace cyber::ui
{
struct WebSocketFrame
{
    std::uint8_t opcode = 0;
    std::string text;
};

std::string websocket_accept_key(const std::string& client_key);
Bytes build_ws_text_frame(const std::string& text);
WebSocketFrame parse_ws_frame(const Bytes& frame);
void perform_websocket_server_handshake(SocketHandle socket);
std::string recv_ws_text(SocketHandle socket);
void send_ws_text(SocketHandle socket, const std::string& text);
} // namespace cyber::ui
```

- [ ] **Step 5: Implement minimal WebSocket support**

Create `src/ui/websocket.cpp`:

```cpp
#include "cyber/ui/websocket.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <wincrypt.h>

namespace cyber::ui
{
namespace
{
using NativeSocket = SOCKET;

std::array<std::uint8_t, 20> sha1(const std::string& input)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::array<std::uint8_t, 20> digest{};
    DWORD digest_len = static_cast<DWORD>(digest.size());

    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        throw std::runtime_error("CryptAcquireContextA failed");
    }
    if (!CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash))
    {
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptCreateHash failed");
    }
    if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(input.data()), static_cast<DWORD>(input.size()), 0))
    {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptHashData failed");
    }
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digest_len, 0))
    {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptGetHashParam failed");
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return digest;
}

std::string base64(const std::uint8_t* data, std::size_t size)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2U) / 3U) * 4U);
    for (std::size_t i = 0; i < size; i += 3U)
    {
        const std::uint32_t b0 = data[i];
        const std::uint32_t b1 = (i + 1U < size) ? data[i + 1U] : 0U;
        const std::uint32_t b2 = (i + 2U < size) ? data[i + 2U] : 0U;
        const std::uint32_t triple = (b0 << 16U) | (b1 << 8U) | b2;
        out.push_back(alphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(alphabet[(triple >> 12U) & 0x3FU]);
        out.push_back((i + 1U < size) ? alphabet[(triple >> 6U) & 0x3FU] : '=');
        out.push_back((i + 2U < size) ? alphabet[triple & 0x3FU] : '=');
    }
    return out;
}

void send_all(SocketHandle socket, const char* data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        const int n = send(static_cast<NativeSocket>(socket), data + sent, len - sent, 0);
        if (n <= 0)
        {
            throw std::runtime_error("websocket send failed");
        }
        sent += n;
    }
}

Bytes recv_some(SocketHandle socket)
{
    std::array<char, 4096> buffer{};
    const int n = recv(static_cast<NativeSocket>(socket), buffer.data(),
                       static_cast<int>(buffer.size()), 0);
    if (n <= 0)
    {
        throw std::runtime_error("websocket recv failed");
    }
    return Bytes(buffer.begin(), buffer.begin() + n);
}
} // namespace

std::string websocket_accept_key(const std::string& client_key)
{
    const std::string magic = client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto digest = sha1(magic);
    return base64(digest.data(), digest.size());
}

Bytes build_ws_text_frame(const std::string& text)
{
    Bytes out;
    out.push_back(0x81);
    if (text.size() <= 125U)
    {
        out.push_back(static_cast<std::uint8_t>(text.size()));
    }
    else if (text.size() <= 65535U)
    {
        out.push_back(126);
        out.push_back(static_cast<std::uint8_t>((text.size() >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(text.size() & 0xFFU));
    }
    else
    {
        throw std::runtime_error("websocket text frame too large");
    }
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

WebSocketFrame parse_ws_frame(const Bytes& frame)
{
    if (frame.size() < 2U)
    {
        throw std::runtime_error("websocket frame too short");
    }
    WebSocketFrame parsed;
    parsed.opcode = frame[0] & 0x0FU;
    const bool masked = (frame[1] & 0x80U) != 0;
    std::uint64_t len = frame[1] & 0x7FU;
    std::size_t offset = 2;
    if (len == 126U)
    {
        if (frame.size() < offset + 2U)
        {
            throw std::runtime_error("websocket extended length missing");
        }
        len = (static_cast<std::uint64_t>(frame[offset]) << 8U) | frame[offset + 1U];
        offset += 2U;
    }
    if (!masked)
    {
        throw std::runtime_error("client websocket frame must be masked");
    }
    if (frame.size() < offset + 4U + len)
    {
        throw std::runtime_error("websocket frame payload truncated");
    }
    const std::uint8_t mask[4] = {frame[offset], frame[offset + 1U], frame[offset + 2U],
                                  frame[offset + 3U]};
    offset += 4U;
    parsed.text.reserve(static_cast<std::size_t>(len));
    for (std::size_t i = 0; i < len; ++i)
    {
        parsed.text.push_back(static_cast<char>(frame[offset + i] ^ mask[i % 4U]));
    }
    return parsed;
}

void perform_websocket_server_handshake(SocketHandle socket)
{
    std::string request;
    std::array<char, 1024> buffer{};
    while (request.find("\r\n\r\n") == std::string::npos)
    {
        const int n = recv(static_cast<NativeSocket>(socket), buffer.data(),
                           static_cast<int>(buffer.size()), 0);
        if (n <= 0)
        {
            throw std::runtime_error("websocket handshake recv failed");
        }
        request.append(buffer.data(), static_cast<std::size_t>(n));
        if (request.size() > 8192U)
        {
            throw std::runtime_error("websocket handshake too large");
        }
    }

    const std::string marker = "Sec-WebSocket-Key:";
    const std::size_t marker_pos = request.find(marker);
    if (marker_pos == std::string::npos)
    {
        throw std::runtime_error("websocket key missing");
    }
    std::size_t key_start = marker_pos + marker.size();
    while (key_start < request.size() && (request[key_start] == ' ' || request[key_start] == '\t'))
    {
        ++key_start;
    }
    const std::size_t key_end = request.find("\r\n", key_start);
    if (key_end == std::string::npos || key_end == key_start)
    {
        throw std::runtime_error("websocket key malformed");
    }
    const std::string client_key = request.substr(key_start, key_end - key_start);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << websocket_accept_key(client_key) << "\r\n\r\n";
    const std::string text = response.str();
    send_all(socket, text.data(), static_cast<int>(text.size()));
}

std::string recv_ws_text(SocketHandle socket)
{
    const WebSocketFrame frame = parse_ws_frame(recv_some(socket));
    if (frame.opcode == 0x8U)
    {
        throw std::runtime_error("websocket close frame received");
    }
    if (frame.opcode != 0x1U)
    {
        throw std::runtime_error("only websocket text frames are supported");
    }
    return frame.text;
}

void send_ws_text(SocketHandle socket, const std::string& text)
{
    const Bytes frame = build_ws_text_frame(text);
    send_all(socket, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
}
} // namespace cyber::ui
```

- [ ] **Step 6: Run WebSocket test**

Run:

```powershell
cmake --build build-mingw --target websocket_selftest
.\build-mingw\websocket_selftest.exe
```

Expected: output contains `websocket_selftest: ok`.

- [ ] **Step 7: Commit WebSocket support**

Run:

```powershell
git add CMakeLists.txt include/cyber/ui/websocket.hpp src/ui/websocket.cpp tests/websocket_selftest.cpp
git commit -m "feat: add local websocket support"
```

---

### Task 6: Plain Game Client and UI Bridge

**Files:**
- Create: `include/cyber/ui/ui_bridge.hpp`
- Create: `src/ui/ui_bridge.cpp`
- Create: `include/cyber/game/plain_game_client.hpp`
- Create: `src/game/plain_game_client.cpp`
- Modify: `src/common/role_runtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create UI bridge header**

Create `include/cyber/ui/ui_bridge.hpp`:

```cpp
#pragma once

#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace cyber::ui
{
struct UiCommand
{
    cyber::game::GameMsgType type = cyber::game::GameMsgType::error;
    cyber::Bytes payload;
};

class UiBridge
{
public:
    using CommandHandler = std::function<void(const UiCommand&)>;

    UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler);
    ~UiBridge();

    void run();
    void stop();
    void broadcast_state(const cyber::game::BattleStateSnapshot& snapshot);

private:
    UiCommand parse_json_command(const std::string& text) const;

    std::uint16_t port_ = 0;
    cyber::EntityId self_ = cyber::EntityId::unknown;
    CommandHandler handler_;
    cyber::SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex clients_mutex_;
    std::vector<cyber::SocketHandle> clients_;
};
} // namespace cyber::ui
```

- [ ] **Step 2: Create UI bridge implementation**

Create `src/ui/ui_bridge.cpp`:

```cpp
#include "cyber/ui/ui_bridge.hpp"

#include "cyber/common/net_socket.hpp"
#include "cyber/ui/websocket.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <thread>

namespace cyber::ui
{
namespace
{
std::string extract_string(const std::string& json, const std::string& key)
{
    const std::string marker = "\"" + key + "\":\"";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return "";
    }
    const std::size_t start = pos + marker.size();
    const std::size_t end = json.find('"', start);
    if (end == std::string::npos)
    {
        return "";
    }
    return json.substr(start, end - start);
}

int extract_int(const std::string& json, const std::string& key, int fallback)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return fallback;
    }
    return std::stoi(json.substr(pos + marker.size()));
}

float extract_float(const std::string& json, const std::string& key, float fallback)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return fallback;
    }
    return std::stof(json.substr(pos + marker.size()));
}

bool extract_bool(const std::string& json, const std::string& key, bool fallback)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
    {
        return fallback;
    }
    const std::string value = json.substr(pos + marker.size(), 5);
    if (value.rfind("true", 0) == 0)
    {
        return true;
    }
    if (value.rfind("false", 0) == 0)
    {
        return false;
    }
    return fallback;
}
} // namespace

UiBridge::UiBridge(std::uint16_t port, cyber::EntityId self, CommandHandler handler)
    : port_(port), self_(self), handler_(std::move(handler))
{
}

UiBridge::~UiBridge()
{
    stop();
}

UiCommand UiBridge::parse_json_command(const std::string& text) const
{
    const std::string type = extract_string(text, "type");
    if (type == "join")
    {
        return {cyber::game::GameMsgType::join,
                cyber::game::build_join({self_, extract_string(text, "name")})};
    }
    if (type == "move")
    {
        return {cyber::game::GameMsgType::move,
                cyber::game::build_move({static_cast<std::int8_t>(extract_int(text, "x", 0)),
                                         static_cast<std::int8_t>(extract_int(text, "y", 0))})};
    }
    if (type == "target")
    {
        return {cyber::game::GameMsgType::target,
                cyber::game::build_target({extract_float(text, "angle", 0.0F)})};
    }
    if (type == "shoot")
    {
        return {cyber::game::GameMsgType::shoot,
                cyber::game::build_shoot({extract_bool(text, "shooting", false)})};
    }
    if (type == "name")
    {
        return {cyber::game::GameMsgType::name,
                cyber::game::build_name({extract_string(text, "name")})};
    }
    return {cyber::game::GameMsgType::error, {}};
}
```

Add the remaining `UiBridge` methods using these acceptance rules:

- `run()` listens on `127.0.0.1:<ui_port>`, accepts browser clients, performs WebSocket handshake, and starts a thread per UI client to read commands.
- Each UI client thread calls `recv_ws_text()`, `parse_json_command()`, and `handler_`.
- `broadcast_state()` converts `BattleStateSnapshot` via `game::to_json(snapshot, self_)` and sends text frames to all connected UI clients.
- `stop()` closes the listener and all UI client sockets.

- [ ] **Step 3: Create plain game client header**

Create `include/cyber/game/plain_game_client.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/ui/ui_bridge.hpp"

#include <atomic>
#include <memory>
#include <mutex>

namespace cyber::game
{
class PlainGameClient
{
public:
    PlainGameClient(Config config, std::uint16_t ui_port);
    void run();

private:
    void send_game_message(GameMsgType type, const Bytes& payload);
    void receive_loop();
    void handle_ui_command(const cyber::ui::UiCommand& command);

    Config config_;
    EntityId self_ = EntityId::unknown;
    std::uint16_t ui_port_ = 0;
    SocketHandle v_socket_ = 0;
    std::mutex send_mutex_;
    std::atomic<bool> stopping_{false};
    Logger logger_;
    std::unique_ptr<cyber::ui::UiBridge> bridge_;
};
} // namespace cyber::game
```

- [ ] **Step 4: Create plain game client implementation**

Create `src/game/plain_game_client.cpp`:

```cpp
#include "cyber/game/plain_game_client.hpp"

#include "cyber/common/net_packet.hpp"

#include <filesystem>
#include <iostream>
#include <thread>

namespace cyber::game
{
namespace
{
TcpEndpoint v_endpoint(const Config& config)
{
    return {config.get_string("V_IP"), config.get_u16("V_PORT")};
}
} // namespace

PlainGameClient::PlainGameClient(Config config, std::uint16_t ui_port)
    : config_(std::move(config)),
      self_(config_.get_entity_id("LOCAL_CLIENT_ID")),
      ui_port_(ui_port),
      logger_(std::filesystem::path("logs") / "client_plain_game.log")
{
}

void PlainGameClient::run()
{
    SocketRuntime runtime;
    v_socket_ = connect_tcp(v_endpoint(config_));
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, self_, [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });
    std::thread rx([this]() { receive_loop(); });
    std::thread ui([this]() { bridge_->run(); });

    const std::string default_name = "p" + std::to_string(static_cast<int>(self_));
    send_game_message(GameMsgType::join, build_join({self_, default_name}));
    std::cout << "Client plaintext game connected to V. UI ws://127.0.0.1:" << ui_port_ << '\n';

    ui.join();
    stopping_ = true;
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
    rx.join();
}

void PlainGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    const Bytes message = build_game_message({type, payload});
    const Packet packet = make_packet(MsgType::app, self_, EntityId::v, message);
    send_packet_logged(v_socket_, packet, logger_, "Client", "PlainGameTx");
}

void PlainGameClient::handle_ui_command(const cyber::ui::UiCommand& command)
{
    if (command.type == GameMsgType::error)
    {
        return;
    }
    send_game_message(command.type, command.payload);
}

void PlainGameClient::receive_loop()
{
    while (!stopping_)
    {
        const Packet packet = recv_packet_logged(v_socket_, logger_, "Client", "PlainGameRx");
        if (packet.msg_type != MsgType::app)
        {
            continue;
        }
        const GameMessage message = parse_game_message(packet.payload);
        if (message.type == GameMsgType::state && bridge_)
        {
            bridge_->broadcast_state(parse_state(message.payload));
        }
    }
}
} // namespace cyber::game
```

- [ ] **Step 5: Add CMake sources**

Modify `CMakeLists.txt`:

```cmake
add_library(cyber_common
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/game_protocol.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
    src/game/plain_game_server.cpp
    src/game/plain_game_client.cpp
    src/ui/websocket.cpp
    src/ui/ui_bridge.cpp
)
```

- [ ] **Step 6: Wire client CLI**

Modify `src/common/role_runtime.cpp` includes:

```cpp
#include "cyber/game/plain_game_client.hpp"
```

Replace the temporary `client --game-plain` branch with:

```cpp
if (role == RoleKind::client)
{
    if (ui_port == 0)
    {
        throw std::runtime_error("client --game-plain requires --ui-port");
    }
    cyber::game::PlainGameClient client(config, ui_port);
    client.run();
    return 0;
}
```

- [ ] **Step 7: Build client and server**

Run:

```powershell
cmake --build build-mingw --target client
cmake --build build-mingw --target v_server
```

Expected: both targets build successfully.

- [ ] **Step 8: Commit plain game client**

Run:

```powershell
git add CMakeLists.txt include/cyber/ui/ui_bridge.hpp src/ui/ui_bridge.cpp include/cyber/game/plain_game_client.hpp src/game/plain_game_client.cpp src/common/role_runtime.cpp
git commit -m "feat: add plaintext game client bridge"
```

---

### Task 7: Web Three.js UI Migration

**Files:**
- Create: `web-ui/package.json`
- Create: `web-ui/tsconfig.json`
- Create: `web-ui/vite.config.ts`
- Create: `web-ui/index.html`
- Create: `web-ui/src/Network.ts`
- Create: `web-ui/src/Game.ts`
- Create: `web-ui/src/Tank.ts`
- Create: `web-ui/src/MapRenderer.ts`
- Create: `web-ui/src/Sound.ts`
- Copy assets: `web-ui/public/models/*`

- [ ] **Step 1: Copy the demo UI scaffold**

Copy from `E:\zhuomian\realtime-tanks-demo\web-threejs` into `E:\zhuomian\cybersecurity\code\web-ui`:

```powershell
New-Item -ItemType Directory -Force .\web-ui
Copy-Item -Recurse -Force E:\zhuomian\realtime-tanks-demo\web-threejs\src .\web-ui\
Copy-Item -Recurse -Force E:\zhuomian\realtime-tanks-demo\web-threejs\public .\web-ui\
Copy-Item -Force E:\zhuomian\realtime-tanks-demo\web-threejs\index.html .\web-ui\index.html
Copy-Item -Force E:\zhuomian\realtime-tanks-demo\web-threejs\package.json .\web-ui\package.json
Copy-Item -Force E:\zhuomian\realtime-tanks-demo\web-threejs\tsconfig.json .\web-ui\tsconfig.json
Copy-Item -Force E:\zhuomian\realtime-tanks-demo\web-threejs\vite.config.ts .\web-ui\vite.config.ts
```

- [ ] **Step 2: Replace Network.ts**

Replace `web-ui/src/Network.ts`:

```ts
export type TeamState = { teamId: number; score: number; tanks: number };
export type TankState = {
  clientId: number;
  name: string;
  team: number;
  x: number;
  y: number;
  angle: number;
  hp: number;
  shield: number;
  dead: boolean;
  score: number;
};
export type BulletState = {
  id: number;
  ownerClientId: number;
  x: number;
  y: number;
  special: boolean;
};
export type PickableState = { id: number; type: number; x: number; y: number };
export type BattleState = {
  serverTimeMs: number;
  totalScore: number;
  winnerTeam: number;
  teams: TeamState[];
  tanks: TankState[];
  bullets: BulletState[];
  pickables: PickableState[];
};

export class Network {
  socket!: WebSocket;
  self = 0;
  state?: BattleState;
  onState?: (state: BattleState) => void;

  constructor(private serverUrl: string) {}

  async connect(): Promise<Network> {
    this.socket = new WebSocket(this.serverUrl);
    this.socket.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === "state") {
        this.self = message.self;
        this.state = message.state;
        this.onState?.(message.state);
      }
    };
    await new Promise<void>((resolve, reject) => {
      this.socket.onopen = () => resolve();
      this.socket.onerror = () => reject(new Error("WebSocket connection failed"));
    });
    return this;
  }

  private send(value: unknown) {
    if (this.socket?.readyState === WebSocket.OPEN) {
      this.socket.send(JSON.stringify(value));
    }
  }

  sendMove(x: number, y: number) {
    this.send({ type: "move", x, y });
  }

  sendTarget(angle: number) {
    this.send({ type: "target", angle });
  }

  sendShoot(shooting: boolean) {
    this.send({ type: "shoot", shooting });
  }

  sendName(name: string) {
    this.send({ type: "name", name });
  }
}
```

- [ ] **Step 3: Update Game.ts imports and connection URL**

In `web-ui/src/Game.ts`, replace the Colyseus imports:

```ts
import { Network, BattleState } from "./Network";
```

Remove `Callbacks` and `Room` usage. In the constructor, set the URL:

```ts
const params = new URLSearchParams(window.location.search);
const serverUrl = params.get("client") || "ws://127.0.0.1:7001";
this.network = new Network(serverUrl);
```

In `start()`, after `await this.network.connect()`, set:

```ts
this.mySessionId = String(this.network.self);
this.network.onState = (state) => this.reconcileState(state);
```

- [ ] **Step 4: Replace Colyseus callback binding with snapshot reconciliation**

Add this method to `Game.ts`:

```ts
private reconcileState(state: BattleState) {
  const seenTanks = new Set<string>();
  for (const tank of state.tanks) {
    const key = String(tank.clientId);
    seenTanks.add(key);
    let entity = this.tanks.get(key);
    if (!entity) {
      entity = new TankEntity(tank.team);
      entity.group.position.set(tank.x, 0, tank.y);
      this.scene.add(entity.group);
      this.tanks.set(key, entity);
    }
    entity.targetX = tank.x;
    entity.targetZ = tank.y;
    if (key !== this.mySessionId) entity.targetAngle = tank.angle;
    entity.setHealth(tank.hp);
    entity.setShield(tank.shield);
    entity.setDead(tank.dead);
  }
  for (const [key, entity] of this.tanks) {
    if (!seenTanks.has(key)) {
      this.scene.remove(entity.group);
      entity.dispose();
      this.tanks.delete(key);
    }
  }

  const seenBullets = new Set<string>();
  for (const bullet of state.bullets) {
    const key = String(bullet.id);
    seenBullets.add(key);
    let mesh = this.bulletMeshes.get(key);
    if (!mesh) {
      const geo = new THREE.SphereGeometry(bullet.special ? 0.2 : 0.12, 6, 6);
      const mat = new THREE.MeshBasicMaterial({ color: bullet.special ? 0xff8800 : 0xffff66 });
      mesh = new THREE.Mesh(geo, mat);
      this.scene.add(mesh);
      this.bulletMeshes.set(key, mesh);
    }
    (mesh as any)._sx = bullet.x;
    (mesh as any)._sy = bullet.y;
    mesh.position.set(bullet.x, 1.5, bullet.y);
  }
  for (const [key, mesh] of this.bulletMeshes) {
    if (!seenBullets.has(key)) {
      this.scene.remove(mesh);
      mesh.geometry.dispose();
      this.bulletMeshes.delete(key);
    }
  }

  this.updateScoresFromSnapshot(state);
  if (state.winnerTeam >= 0) this.showWinnerScreen(state.winnerTeam);
}
```

Add `updateScoresFromSnapshot(state: BattleState)` that mirrors existing `updateScores()` but reads `state.teams`.

- [ ] **Step 5: Build Web UI**

Run:

```powershell
cd web-ui
npm install
npm run build
```

Expected: Vite build succeeds.

- [ ] **Step 6: Commit Web UI**

Run:

```powershell
git add web-ui
git commit -m "feat: add browser tank UI for plaintext client"
```

---

### Task 8: Documentation and Full Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/four-host-connect-test.md` or create `docs/plaintext-tank-application.md`

- [ ] **Step 1: Document Phase A startup**

Add to `README.md`:

~~~markdown
## Phase A Plaintext Tank Battle

Start V on the V host:

```powershell
.\build-mingw\v_server.exe --game-plain
```

Start one client per player host:

```powershell
.\build-mingw\client.exe --game-plain --ui-port 7001
```

Start the Web UI:

```powershell
cd web-ui
npm install
npm run dev -- --host 127.0.0.1
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001
```

Phase A is plaintext only. AS/TGS, DES, RSA signatures, ACK non-repudiation, and replay protection are not used in this mode.
~~~

- [ ] **Step 2: Run all C++ tests**

Run:

```powershell
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

Expected: all registered tests pass, including:

```text
game_protocol_selftest
game_world_selftest
battle_room_selftest
websocket_selftest
plain_game_flow_selftest
```

- [ ] **Step 3: Run Web UI build**

Run:

```powershell
cd web-ui
npm run build
```

Expected: Vite build succeeds.

- [ ] **Step 4: Manual smoke test**

Run three terminals:

```powershell
.\build-mingw\v_server.exe --game-plain
```

```powershell
.\build-mingw\client.exe --game-plain --ui-port 7001
```

```powershell
cd web-ui
npm run dev -- --host 127.0.0.1
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001
```

Expected manual result:

- Browser connects without console WebSocket errors.
- One tank appears after client auto-join.
- WASD moves the tank.
- Mouse aim changes turret direction.
- Left click creates bullets.
- V terminal logs client connection and game traffic.

- [ ] **Step 5: Commit docs and verification notes**

Run:

```powershell
git add README.md docs
git commit -m "docs: describe plaintext tank application mode"
```
