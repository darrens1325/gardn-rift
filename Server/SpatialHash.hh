#pragma once

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <cstdint>
#include <functional>
#include <vector>

class Simulation;
class Entity;

static const uint32_t GRID_SIZE = 100 * 2;

class SpatialHash {
    Simulation *simulation;
    // Flat row-major grid: cells[x * grid_y_cap + y]. The capacity grows
    // monotonically — it tracks the largest arena we've seen since startup
    // so re-allocating the backing vector (and invalidating x*y_cap
    // indexing) only happens when the arena actually grows past a prior
    // high-water mark. ARENA_WIDTH / ARENA_HEIGHT are runtime now (see
    // Shared/MapDimensions.hh) because TiledMap::load() derives them from
    // whatever .tmj it reads at startup.
    std::vector<std::vector<EntityID>> cells;
    uint32_t grid_x_cap;
    uint32_t grid_y_cap;
    uint32_t width;
    uint32_t height;
public:
    SpatialHash(Simulation *);
    void refresh(uint32_t, uint32_t);
    void insert(Entity const &);
    void collide(std::function<void(Simulation *, Entity &, Entity &)>);
    void query(float, float, float, float, std::function<void(Simulation *, Entity &)>);
};
