#include "cyber/game/game_world.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cyber::game
{
// 构造矩形墙块，并设置空间索引用的中心点和包围半径。
Block::Block(float center_x, float center_y, float width, float height)
{
    x = center_x;
    y = center_y;
    radius = std::max(width, height) * 0.5F;
    width_ = width;
    height_ = height;
}

// 检测圆形对象与矩形墙块的碰撞，并返回应推出的位移。
std::optional<Vec2> Block::collide_circle(float cx, float cy, float circle_radius) const
{
    // 步骤 1：根据墙块中心点和宽高，计算矩形四条边界。
    const float left = x - width_ * 0.5F;
    const float right = x + width_ * 0.5F;
    const float top = y - height_ * 0.5F;
    const float bottom = y + height_ * 0.5F;

    // 步骤 2：把圆心投影/夹取到矩形范围内，得到矩形上离圆心最近的点。
    // 如果圆心在矩形外，这个点位于矩形边或角；如果圆心在矩形内，这个点就是圆心本身。
    const float nearest_x = std::max(left, std::min(cx, right));
    const float nearest_y = std::max(top, std::min(cy, bottom));

    // 步骤 3：计算圆心到最近点的距离平方。
    // 距离大于等于圆半径，说明圆和矩形没有重叠。
    const float dx = cx - nearest_x;
    const float dy = cy - nearest_y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 >= circle_radius * circle_radius)
    {
        return std::nullopt;
    }
    if (dist2 > 0.0001F)
    {
        // 步骤 4：普通情况。圆心在矩形外部，且圆已经压进矩形边缘。
        // 沿“最近点 -> 圆心”的方向，把圆推出刚好不重叠的位置。
        const float dist = std::sqrt(dist2);
        const float push = circle_radius - dist;
        return Vec2{(dx / dist) * push, (dy / dist) * push};
    }

    // 步骤 5：特殊情况。圆心已经落在矩形内部，最近点就是圆心，无法用 dx/dy 判断方向。
    // 此时计算圆心到四条边的距离，选择最近的一条边作为推出方向。
    const float push_left = std::abs(cx - left);
    const float push_right = std::abs(right - cx);
    const float push_top = std::abs(cy - top);
    const float push_bottom = std::abs(bottom - cy);
    const float best = std::min(std::min(push_left, push_right), std::min(push_top, push_bottom));
    if (best == push_left)
    {
        // 步骤 5.1：离左边最近，向左推出。
        return Vec2{-(circle_radius + push_left), 0.0F};
    }
    if (best == push_right)
    {
        // 步骤 5.2：离右边最近，向右推出。
        return Vec2{circle_radius + push_right, 0.0F};
    }
    if (best == push_top)
    {
        // 步骤 5.3：离上边最近，向上推出。
        return Vec2{0.0F, -(circle_radius + push_top)};
    }
    // 步骤 5.4：离下边最近，向下推出。
    return Vec2{0.0F, circle_radius + push_bottom};
}

// 返回墙块宽度。
float Block::width() const
{
    return width_;
}

// 返回墙块高度。
float Block::height() const
{
    return height_;
}

// 初始化世界空间索引，按 cluster_size 创建二维网格。
World::World(float width, float height, float cluster_size)
    : width_(width), height_(height), cluster_size_(cluster_size)
{
    if (width_ <= 0.0F || height_ <= 0.0F || cluster_size_ <= 0.0F)
    {
        throw std::runtime_error("invalid world dimensions");
    }
    cluster_width_ = std::max(1, static_cast<int>(std::ceil(width_ / cluster_size_)));
    cluster_height_ = std::max(1, static_cast<int>(std::ceil(height_ / cluster_size_)));
    for (LayerData& layer_data : layers_)
    {
        layer_data.nodes.resize(static_cast<std::size_t>(cluster_width_ * cluster_height_));
    }
}

// 根据坐标计算所在网格编号，坐标越界时夹到边界格。
int World::pick(float px, float py) const
{
    const int ix = std::max(0, std::min(cluster_width_ - 1, static_cast<int>(px / cluster_size_)));
    const int iy = std::max(0, std::min(cluster_height_ - 1, static_cast<int>(py / cluster_size_)));
    return iy * cluster_width_ + ix;
}

// 返回指定对象层的空间索引数据。
World::LayerData& World::layer(SpatialLayer layer_name)
{
    return layers_[static_cast<std::size_t>(layer_name)];
}

// 将对象加入指定层的当前坐标网格。
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

// 将对象从指定层的旧网格中移除。
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

// 在对象移动后更新它所在的网格位置。
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

// 遍历指定对象周围九宫格里的同层对象。
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

// 返回世界宽度。
float World::width() const
{
    return width_;
}

// 返回世界高度。
float World::height() const
{
    return height_;
}

// 返回固定地图的墙块列表。
const std::vector<Block>& level_blocks()
{
    static const std::vector<Block> blocks = {
        {13.5F, 2.0F, 1.0F, 4.0F},  {13.5F, 12.0F, 1.0F, 2.0F},
        {12.5F, 13.5F, 3.0F, 1.0F}, {2.0F, 13.5F, 4.0F, 1.0F},
        {11.5F, 15.0F, 1.0F, 2.0F}, {11.5F, 23.5F, 1.0F, 5.0F},
        {10.0F, 26.5F, 4.0F, 1.0F}, {6.0F, 26.5F, 4.0F, 1.0F},
        {2.0F, 34.5F, 4.0F, 1.0F},  {12.5F, 34.5F, 3.0F, 1.0F},
        {13.5F, 36.0F, 1.0F, 2.0F}, {15.0F, 36.5F, 2.0F, 1.0F},
        {13.5F, 46.0F, 1.0F, 4.0F}, {23.5F, 36.5F, 5.0F, 1.0F},
        {26.5F, 38.0F, 1.0F, 4.0F}, {26.5F, 42.0F, 1.0F, 4.0F},
        {34.5F, 46.0F, 1.0F, 4.0F}, {34.5F, 36.0F, 1.0F, 2.0F},
        {35.5F, 34.5F, 3.0F, 1.0F}, {36.5F, 33.0F, 1.0F, 2.0F},
        {46.0F, 34.5F, 4.0F, 1.0F}, {36.5F, 24.5F, 1.0F, 5.0F},
        {38.0F, 21.5F, 4.0F, 1.0F}, {42.0F, 21.5F, 4.0F, 1.0F},
        {46.0F, 13.5F, 4.0F, 1.0F}, {35.5F, 13.5F, 3.0F, 1.0F},
        {34.5F, 12.0F, 1.0F, 2.0F}, {33.0F, 11.5F, 2.0F, 1.0F},
        {34.5F, 2.0F, 1.0F, 4.0F},  {24.5F, 11.5F, 5.0F, 1.0F},
        {21.5F, 10.0F, 1.0F, 4.0F}, {21.5F, 6.0F, 1.0F, 4.0F},
        {18.5F, 22.0F, 1.0F, 6.0F}, {19.0F, 18.5F, 2.0F, 1.0F},
        {26.0F, 18.5F, 6.0F, 1.0F}, {29.5F, 19.0F, 1.0F, 2.0F},
        {29.5F, 26.0F, 1.0F, 6.0F}, {29.0F, 29.5F, 2.0F, 1.0F},
        {22.0F, 29.5F, 6.0F, 1.0F}, {18.5F, 29.0F, 1.0F, 2.0F}};
    return blocks;
}

// 返回固定地图的补给刷新点列表。
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
