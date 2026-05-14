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

        const cyber::ui::UiCommand join = bridge.parse_json_command(
            "{\"type\":\"join\",\"name\":\"bravo\"}");
        require(join.type == cyber::game::GameMsgType::join, "join command type mismatch");
        require(cyber::game::parse_join(join.payload).client_id == cyber::EntityId::client2,
                "join client mismatch");

        const cyber::ui::UiCommand move =
            bridge.parse_json_command("{\"type\":\"move\",\"x\":1,\"y\":-1}");
        const cyber::game::MoveMessage parsed_move = cyber::game::parse_move(move.payload);
        require(move.type == cyber::game::GameMsgType::move, "move command type mismatch");
        require(parsed_move.x == 1 && parsed_move.y == -1, "move payload mismatch");

        const cyber::ui::UiCommand target =
            bridge.parse_json_command("{\"type\":\"target\",\"angle\":135.5}");
        require(target.type == cyber::game::GameMsgType::target, "target command type mismatch");
        require_close(cyber::game::parse_target(target.payload).angle, 135.5F,
                      "target payload mismatch");

        const cyber::ui::UiCommand shoot =
            bridge.parse_json_command("{\"type\":\"shoot\",\"shooting\":true}");
        require(shoot.type == cyber::game::GameMsgType::shoot, "shoot command type mismatch");
        require(cyber::game::parse_shoot(shoot.payload).shooting, "shoot payload mismatch");

        std::cout << "ui_bridge_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "ui_bridge_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
