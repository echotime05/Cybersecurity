#include "cyber/ui/ui_bridge.hpp"

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
        cyber::ui::UiBridge bridge(0, cyber::EntityId::client2,
                                   [](const cyber::ui::UiCommand&) {});

        const cyber::ui::UiCommand login =
            bridge.parse_json_command("{\"type\":\"login\",\"clientId\":4,\"password\":\"&wxh@147\"}");
        require(login.kind == cyber::ui::UiCommandKind::login, "login command kind mismatch");
        require(login.client_id == cyber::EntityId::client4, "login client mismatch");
        require(login.password == "&wxh@147", "login password mismatch");

        const cyber::ui::UiCommand join_gate = bridge.parse_json_command("{\"type\":\"join\"}");
        require(join_gate.kind == cyber::ui::UiCommandKind::join_game,
                "join gate command kind mismatch");

        const cyber::ui::UiCommand move =
            bridge.parse_json_command("{\"type\":\"move\",\"x\":1,\"y\":-1}");
        const cyber::game::MoveMessage parsed_move = cyber::game::game_parse_move(move.payload);
        require(move.kind == cyber::ui::UiCommandKind::game, "move command kind mismatch");
        require(move.type == cyber::game::GameMsgType::move, "move command type mismatch");
        require(parsed_move.x == 1 && parsed_move.y == -1, "move payload mismatch");

        const cyber::ui::UiCommand target =
            bridge.parse_json_command("{\"type\":\"target\",\"angle\":135.5}");
        require(target.type == cyber::game::GameMsgType::target, "target command type mismatch");
        require_close(cyber::game::game_parse_target(target.payload).angle, 135.5F,
                      "target payload mismatch");

        const cyber::ui::UiCommand shoot =
            bridge.parse_json_command("{\"type\":\"shoot\"}");
        require(shoot.type == cyber::game::GameMsgType::shoot, "shoot command type mismatch");
        require(shoot.payload.empty(), "shoot payload should be empty");
        (void)cyber::game::game_parse_shoot(shoot.payload);

        const std::string authenticated =
            cyber::ui::login_state_json("authenticated", cyber::EntityId::client4,
                                        "172.27.39.248:9003", "");
        require(authenticated.find("\"type\":\"loginState\"") != std::string::npos,
                "loginState type missing");
        require(authenticated.find("\"status\":\"authenticated\"") != std::string::npos,
                "loginState status missing");
        require(authenticated.find("\"clientId\":4") != std::string::npos,
                "loginState client id missing");
        require(authenticated.find("\"vServer\":\"172.27.39.248:9003\"") != std::string::npos,
                "loginState V server missing");

        const std::string failed =
            cyber::ui::login_state_json("failed", cyber::EntityId::unknown, "",
                                        "Invalid client id or password");
        require(failed.find("\"message\":\"Invalid client id or password\"") != std::string::npos,
                "loginState failure message missing");
        require(cyber::ui::join_state_json("joined").find("\"status\":\"joined\"") !=
                    std::string::npos,
                "joinState JSON mismatch");

        std::cout << "ui_bridge_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "ui_bridge_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
