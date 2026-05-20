#include "cyber/game/game_protocol.hpp"

#include "cyber/protocol/binary_codec.hpp"

#include <sstream>
#include <stdexcept>

namespace cyber::game
{
namespace
{
using cyber::protocol::detail::binary_read_f32;
using cyber::protocol::detail::binary_read_u16;
using cyber::protocol::detail::binary_read_u64;
using cyber::protocol::detail::binary_require_end;
using cyber::protocol::detail::binary_write_f32;
using cyber::protocol::detail::binary_write_u16;
using cyber::protocol::detail::binary_write_u64;

// 将一辆坦克快照写入 GAME_STATE payload。
void write_tank(Bytes& out, const TankSnapshot& tank)
{
    out.push_back(static_cast<std::uint8_t>(tank.client_id));
    out.push_back(tank.team);
    binary_write_f32(out, tank.x);
    binary_write_f32(out, tank.y);
    binary_write_f32(out, tank.angle);
    out.push_back(static_cast<std::uint8_t>(tank.hp));
    out.push_back(static_cast<std::uint8_t>(tank.shield));
    out.push_back(static_cast<std::uint8_t>(tank.dead ? 1U : 0U));
    binary_write_u16(out, tank.score);
}

// 从 GAME_STATE payload 读取一辆坦克快照。
TankSnapshot read_tank(const Bytes& in, std::size_t& offset)
{
    if (offset >= in.size())
    {
        throw PacketError("tank client id missing");
    }
    TankSnapshot tank;
    tank.client_id = static_cast<EntityId>(in[offset++]);
    if (offset + 1U + 4U + 4U + 4U + 1U + 1U + 1U + 2U > in.size())
    {
        throw PacketError("tank payload is truncated");
    }
    tank.team = in[offset++];
    tank.x = binary_read_f32(in, offset, "game payload");
    tank.y = binary_read_f32(in, offset, "game payload");
    tank.angle = binary_read_f32(in, offset, "game payload");
    tank.hp = static_cast<std::int8_t>(in[offset++]);
    tank.shield = static_cast<std::int8_t>(in[offset++]);
    tank.dead = in[offset++] != 0;
    tank.score = binary_read_u16(in, offset, "game payload");
    return tank;
}
} // namespace

// 序列化游戏层通用消息：GameMsgType + 具体 payload。
Bytes game_build_message(const GameMessage& message)
{
    Bytes out;
    out.reserve(1U + message.payload.size());
    out.push_back(static_cast<std::uint8_t>(message.type));
    out.insert(out.end(), message.payload.begin(), message.payload.end());
    return out;
}

// 解析游戏层通用消息。
GameMessage game_parse_message(const Bytes& bytes)
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

// 序列化加入游戏请求。
Bytes game_build_join(const JoinMessage& message)
{
    return Bytes{static_cast<std::uint8_t>(message.client_id)};
}

// 解析加入游戏请求。
JoinMessage game_parse_join(const Bytes& bytes)
{
    if (bytes.size() != 1U)
    {
        throw PacketError("join payload must be 1 byte");
    }
    std::size_t offset = 0;
    JoinMessage message;
    message.client_id = static_cast<EntityId>(bytes[offset++]);
    binary_require_end(bytes, offset, "join");
    return message;
}

// 序列化移动输入消息。
Bytes game_build_move(const MoveMessage& message)
{
    return Bytes{static_cast<std::uint8_t>(message.x), static_cast<std::uint8_t>(message.y)};
}

// 解析移动输入消息。
MoveMessage game_parse_move(const Bytes& bytes)
{
    if (bytes.size() != 2U)
    {
        throw PacketError("move payload must be 2 bytes");
    }
    return {static_cast<std::int8_t>(bytes[0]), static_cast<std::int8_t>(bytes[1])};
}

// 序列化炮塔朝向消息。
Bytes game_build_target(const TargetMessage& message)
{
    Bytes out;
    binary_write_f32(out, message.angle);
    return out;
}

// 解析炮塔朝向消息。
TargetMessage game_parse_target(const Bytes& bytes)
{
    std::size_t offset = 0;
    TargetMessage message{binary_read_f32(bytes, offset, "game payload")};
    binary_require_end(bytes, offset, "target");
    return message;
}

// 序列化单次开火消息，当前没有额外 payload 字段。
Bytes game_build_shoot(const ShootMessage& message)
{
    (void)message;
    return {};
}

// 解析单次开火消息，要求 payload 为空。
ShootMessage game_parse_shoot(const Bytes& bytes)
{
    if (!bytes.empty())
    {
        throw PacketError("shoot payload must be empty");
    }
    return {};
}

// 序列化完整世界快照，V 周期性广播该 payload。
Bytes game_build_state(const BattleStateSnapshot& snapshot)
{
    Bytes out;
    binary_write_u64(out, snapshot.server_time_ms);
    binary_write_u16(out, snapshot.total_score);
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
        binary_write_u16(out, team.score);
        out.push_back(team.tanks);
    }
    out.push_back(static_cast<std::uint8_t>(snapshot.tanks.size()));
    for (const TankSnapshot& tank : snapshot.tanks)
    {
        write_tank(out, tank);
    }
    binary_write_u16(out, static_cast<std::uint16_t>(snapshot.bullets.size()));
    for (const BulletSnapshot& bullet : snapshot.bullets)
    {
        binary_write_u16(out, bullet.id);
        out.push_back(static_cast<std::uint8_t>(bullet.owner_client_id));
        binary_write_f32(out, bullet.x);
        binary_write_f32(out, bullet.y);
        out.push_back(static_cast<std::uint8_t>(bullet.special ? 1U : 0U));
    }
    binary_write_u16(out, static_cast<std::uint16_t>(snapshot.pickables.size()));
    for (const PickableSnapshot& pickable : snapshot.pickables)
    {
        binary_write_u16(out, pickable.id);
        out.push_back(static_cast<std::uint8_t>(pickable.type));
        binary_write_f32(out, pickable.x);
        binary_write_f32(out, pickable.y);
    }
    return out;
}

// 解析完整世界快照，Client 收到 GAME_STATE 后使用。
BattleStateSnapshot game_parse_state(const Bytes& bytes)
{
    std::size_t offset = 0;
    BattleStateSnapshot snapshot;
    snapshot.server_time_ms = binary_read_u64(bytes, offset, "game payload");
    snapshot.total_score = binary_read_u16(bytes, offset, "game payload");
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
        team.score = binary_read_u16(bytes, offset, "game payload");
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
    const std::uint16_t bullet_count = binary_read_u16(bytes, offset, "game payload");
    for (std::uint16_t i = 0; i < bullet_count; ++i)
    {
        BulletSnapshot bullet;
        bullet.id = binary_read_u16(bytes, offset, "game payload");
        if (offset >= bytes.size())
        {
            throw PacketError("bullet owner missing");
        }
        bullet.owner_client_id = static_cast<EntityId>(bytes[offset++]);
        bullet.x = binary_read_f32(bytes, offset, "game payload");
        bullet.y = binary_read_f32(bytes, offset, "game payload");
        if (offset >= bytes.size())
        {
            throw PacketError("bullet special missing");
        }
        bullet.special = bytes[offset++] != 0;
        snapshot.bullets.push_back(bullet);
    }
    const std::uint16_t pickable_count = binary_read_u16(bytes, offset, "game payload");
    for (std::uint16_t i = 0; i < pickable_count; ++i)
    {
        PickableSnapshot pickable;
        pickable.id = binary_read_u16(bytes, offset, "game payload");
        if (offset >= bytes.size())
        {
            throw PacketError("pickable type missing");
        }
        pickable.type = static_cast<PickableType>(bytes[offset++]);
        pickable.x = binary_read_f32(bytes, offset, "game payload");
        pickable.y = binary_read_f32(bytes, offset, "game payload");
        snapshot.pickables.push_back(pickable);
    }
    binary_require_end(bytes, offset, "state");
    return snapshot;
}

// 将世界快照格式化为 Web UI 渲染用 JSON。
std::string game_format_state_json(const BattleStateSnapshot& snapshot, EntityId self)
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
        if (i != 0)
        {
            out << ',';
        }
        out << "{\"teamId\":" << static_cast<int>(team.team_id) << ",\"score\":" << team.score
            << ",\"tanks\":" << static_cast<int>(team.tanks) << '}';
    }
    out << "],\"tanks\":[";
    for (std::size_t i = 0; i < snapshot.tanks.size(); ++i)
    {
        const TankSnapshot& tank = snapshot.tanks[i];
        if (i != 0)
        {
            out << ',';
        }
        out << "{\"clientId\":" << static_cast<int>(tank.client_id)
            << ",\"team\":" << static_cast<int>(tank.team)
            << ",\"x\":" << tank.x << ",\"y\":" << tank.y << ",\"angle\":" << tank.angle
            << ",\"hp\":" << static_cast<int>(tank.hp)
            << ",\"shield\":" << static_cast<int>(tank.shield)
            << ",\"dead\":" << (tank.dead ? "true" : "false") << ",\"score\":" << tank.score
            << '}';
    }
    out << "],\"bullets\":[";
    for (std::size_t i = 0; i < snapshot.bullets.size(); ++i)
    {
        const BulletSnapshot& bullet = snapshot.bullets[i];
        if (i != 0)
        {
            out << ',';
        }
        out << "{\"id\":" << bullet.id
            << ",\"ownerClientId\":" << static_cast<int>(bullet.owner_client_id)
            << ",\"x\":" << bullet.x << ",\"y\":" << bullet.y
            << ",\"special\":" << (bullet.special ? "true" : "false") << '}';
    }
    out << "],\"pickables\":[";
    for (std::size_t i = 0; i < snapshot.pickables.size(); ++i)
    {
        const PickableSnapshot& pickable = snapshot.pickables[i];
        if (i != 0)
        {
            out << ',';
        }
        out << "{\"id\":" << pickable.id << ",\"type\":"
            << static_cast<int>(pickable.type) << ",\"x\":" << pickable.x
            << ",\"y\":" << pickable.y << '}';
    }
    out << "]}}";
    return out.str();
}
} // namespace cyber::game
