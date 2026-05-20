#pragma once

#include "cyber/game/game_protocol.hpp"
#include "cyber/game/game_types.hpp"
#include "cyber/game/game_world.hpp"

#include <array>
#include <map>
#include <mutex>
#include <vector>

namespace cyber::game
{
// V 服务器内的战斗房间，负责玩家加入、输入处理、碰撞、得分和快照生成。
class BattleRoom
{
public:
    // 初始化地图、队伍和补给点。
    BattleRoom();

    // 将 Client 加入房间并分配出生位置，已加入时保持幂等。
    bool join(EntityId client_id, std::uint64_t now_ms);
    // 将 Client 标记离开房间，并清理对应坦克状态。
    void leave(EntityId client_id);
    // 更新某个 Client 的移动输入方向。
    void handle_move(EntityId client_id, std::int8_t x, std::int8_t y);
    // 更新某个 Client 的炮塔朝向。
    void handle_target(EntityId client_id, float angle);
    // 处理某个 Client 的单次开火请求。
    void handle_shoot(EntityId client_id);
    // 推进一帧游戏世界，处理移动、子弹、补给、复活和胜利状态。
    void tick(std::uint64_t now_ms);
    // 生成当前完整世界快照，供 V 广播给所有 Client。
    BattleStateSnapshot snapshot(std::uint64_t now_ms) const;

private:
    // 空间索引中的坦克对象，同时持有业务状态。
    struct TankItem : SpatialItem
    {
        TankState state;
    };

    // 空间索引中的子弹对象，同时持有业务状态。
    struct BulletItem : SpatialItem
    {
        BulletState state;
    };

    // 空间索引中的补给对象，同时持有业务状态。
    struct PickableItem : SpatialItem
    {
        PickableState state;
    };

    // 选择当前人数最少的队伍，用于新玩家分队。
    std::uint8_t pick_weakest_team() const;
    // 为坦克计算出生点。
    void spawn_position(TankState& tank) const;
    // 从坦克位置和朝向创建一颗子弹。
    void create_bullet(TankItem& tank, std::uint64_t now_ms);
    // 根据当前坦克、子弹、补给和墙块重建空间索引。
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
