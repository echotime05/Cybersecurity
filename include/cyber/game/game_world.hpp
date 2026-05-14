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
