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
// V 服务器内的权威战斗房间。
// Worker 线程收到 Client 的 GAME_JOIN_REQ、GAME_MOVE、GAME_TARGET、GAME_SHOOT 后，
// 只调用 BattleRoom 的 handle_* 接口修改输入或加入状态；真正的移动、碰撞、
// 子弹飞行、补给拾取、死亡复活和胜负判定都在游戏线程的 tick() 中统一推进。
// 对外广播的 GAME_STATE 由 snapshot() 从这里的权威状态生成，Client 只负责渲染。
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
    // 坦克在 V 内部的物理对象。
    // SpatialItem 字段用于空间索引和碰撞，state 字段保存游戏业务状态和快照数据。
    struct TankItem : SpatialItem
    {
        TankState state;
    };

    // 子弹在 V 内部的物理对象。
    // SpatialItem 字段用于移动碰撞，state 字段保存归属、伤害和网络快照数据。
    struct BulletItem : SpatialItem
    {
        BulletState state;
    };

    // 补给在 V 内部的物理对象。
    // SpatialItem 字段用于与坦克碰撞，state 字段保存补给类型和刷新点关联。
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

    // 保护 BattleRoom 内部权威状态；网络线程和游戏线程都会访问这些容器。
    mutable std::mutex mutex_;
    // 四个队伍的当前分数和存活数量。
    std::array<TeamState, 4> teams_{};
    // 所有已加入房间的坦克，key 是 Client ID。
    std::map<EntityId, TankItem> tanks_;
    // 当前正在飞行的子弹，key 是 BulletState::id。
    std::map<std::uint16_t, BulletItem> bullets_;
    // 当前地图上已经刷出的补给，key 是 PickableState::id。
    std::map<std::uint16_t, PickableItem> pickables_;
    // 固定地图墙块。
    std::vector<Block> blocks_;
    // 固定补给刷新点及其运行状态。
    std::vector<PickableSpawn> pick_spawns_;
    // 空间索引，用于降低邻近对象碰撞查询成本。
    World world_;
    // 子弹 ID 递增计数器。
    std::uint16_t bullet_counter_ = 0;
    // 补给 ID 递增计数器。
    std::uint16_t pick_counter_ = 0;
    // 全局总分，用于快照展示。
    std::uint16_t total_score_ = 0;
    // 当前获胜队伍，-1 表示尚未产生胜者。
    std::int8_t winner_team_ = -1;
    // 记录胜者产生时间，用于后续重置或展示控制。
    std::uint64_t winner_set_ms_ = 0;
};
} // namespace cyber::game
