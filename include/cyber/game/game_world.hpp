#pragma once

#include "cyber/game/game_types.hpp"

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace cyber::game
{
// 可放入空间索引的对象基类，坦克、子弹、补给和墙块都复用这组字段。
struct SpatialItem
{
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.0F;
    int node_index = -1;
};

// 空间索引按对象类型分层，避免子弹和墙块等不同对象互相误遍历。
enum class SpatialLayer : std::uint8_t
{
    tank = 0,
    bullet = 1,
    pickable = 2,
    block = 3
};

// 地图中的矩形障碍物，提供圆形对象碰撞检测。
class Block : public SpatialItem
{
public:
    // 构造以中心点和宽高表示的墙块。
    Block(float center_x, float center_y, float width, float height);
    // 检测圆形对象是否与墙块碰撞，碰撞时返回推出方向。
    std::optional<Vec2> collide_circle(float cx, float cy, float radius) const;

    // 返回墙块宽度。
    float width() const;
    // 返回墙块高度。
    float height() const;

private:
    float width_ = 0.0F;
    float height_ = 0.0F;
};

// 简单网格空间索引，V 的物理和碰撞逻辑用它降低邻近对象遍历成本。
class World
{
public:
    // 按世界宽高和网格尺寸初始化空间索引。
    World(float width, float height, float cluster_size);

    // 将对象加入指定空间层。
    void add(SpatialLayer layer, SpatialItem* item);
    // 从指定空间层移除对象。
    void remove(SpatialLayer layer, SpatialItem& item);
    // 对象位置改变后更新它所在的网格节点。
    void update(SpatialLayer layer, SpatialItem& item);
    // 遍历指定对象附近九宫格内的同层对象。
    void for_each_around(SpatialLayer layer, const SpatialItem& item,
                         const std::function<void(SpatialItem&)>& fn,
                         const SpatialItem* exclude = nullptr);

    // 返回世界宽度。
    float width() const;
    // 返回世界高度。
    float height() const;

private:
    // 单个网格节点，保存落在此格中的对象指针。
    struct Node
    {
        std::vector<SpatialItem*> items;
    };

    // 某一类对象的完整空间层。
    struct LayerData
    {
        std::vector<Node> nodes;
    };

    // 根据坐标计算所在网格索引。
    int pick(float x, float y) const;
    // 取得指定空间层的数据容器。
    LayerData& layer(SpatialLayer layer);

    float width_ = 0.0F;
    float height_ = 0.0F;
    float cluster_size_ = 1.0F;
    int cluster_width_ = 1;
    int cluster_height_ = 1;
    std::array<LayerData, 4> layers_;
};

// 返回固定关卡墙块配置。
const std::vector<Block>& level_blocks();
// 返回固定关卡补给刷新点配置。
const std::vector<PickableSpawn>& pickable_spawns();
} // namespace cyber::game
