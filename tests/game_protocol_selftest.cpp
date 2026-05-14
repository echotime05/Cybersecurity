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
        const cyber::game::GameMessage join_msg{cyber::game::GameMsgType::join, join_payload};
        const cyber::Bytes encoded_join = cyber::game::build_game_message(join_msg);
        const cyber::game::GameMessage parsed_join = cyber::game::parse_game_message(encoded_join);
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

        const std::string json = cyber::game::to_json(parsed_snapshot, cyber::EntityId::client1);
        require(json.find("\"type\":\"state\"") != std::string::npos, "json type missing");
        require(json.find("\"name\":\"alpha\"") != std::string::npos, "json tank missing");

        std::cout << "game_protocol_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "game_protocol_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
