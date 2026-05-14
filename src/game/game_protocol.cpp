#include "cyber/game/game_protocol.hpp"

#include <cstring>
#include <iomanip>
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
        out.push_back(static_cast<std::uint8_t>((value >> static_cast<unsigned>(i * 8)) & 0xFFU));
    }
}

std::uint16_t read_u16(const Bytes& in, std::size_t& offset)
{
    if (offset + 2U > in.size())
    {
        throw PacketError("game payload is too short for uint16");
    }
    const std::uint16_t value =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[offset]) << 8U) |
                                   static_cast<std::uint16_t>(in[offset + 1U]));
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
    std::string out(in.begin() + static_cast<std::ptrdiff_t>(offset),
                    in.begin() + static_cast<std::ptrdiff_t>(offset + len));
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

std::string json_escape(const std::string& text)
{
    std::ostringstream out;
    for (unsigned char ch : text)
    {
        switch (ch)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (ch < 0x20U)
            {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch) << std::dec;
            }
            else
            {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
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
    out.push_back(static_cast<std::uint8_t>(tank.dead ? 1U : 0U));
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
        out.push_back(static_cast<std::uint8_t>(bullet.special ? 1U : 0U));
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
        if (offset >= bytes.size())
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
        out << "{\"clientId\":" << static_cast<int>(tank.client_id) << ",\"name\":\""
            << json_escape(tank.name) << "\",\"team\":" << static_cast<int>(tank.team)
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
