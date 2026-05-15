#include <Server/SpatialHash.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

#include <algorithm>
#include <unordered_set>

static uint32_t _hash_two(EntityID const a, EntityID const b) {
    if (a.id > b.id) return (a.id << 16) + b.id;
    else return (b.id << 16) + a.id;
}

SpatialHash::SpatialHash(Simulation *sim)
    : simulation(sim), grid_x_cap(0), grid_y_cap(0), width(1), height(1) {}

void SpatialHash::refresh(uint32_t _width, uint32_t _height) {
    DEBUG_ONLY(assert(_width <= ARENA_WIDTH && _height <= ARENA_HEIGHT));
    width = div_round_up(_width, GRID_SIZE);
    height = div_round_up(_height, GRID_SIZE);
    if (width > grid_x_cap || height > grid_y_cap) {
        // See SpatialHashUniform.cc refresh — capacity growth implies
        // re-indexing, so reassign the whole backing store.
        grid_x_cap = std::max(grid_x_cap, width);
        grid_y_cap = std::max(grid_y_cap, height);
        cells.assign((size_t)grid_x_cap * grid_y_cap, {});
        return;
    }
    for (uint32_t x = 0; x < width; ++x)
        for (uint32_t y = 0; y < height; ++y)
            cells[(size_t)x * grid_y_cap + y].clear();
}

void SpatialHash::insert(Entity const &ent) {
    DEBUG_ONLY(assert(ent.has_component(kPhysics));)
    uint32_t sx = fclamp(ent.x - ent.radius, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t sy = fclamp(ent.y - ent.radius, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    uint32_t ex = fclamp(ent.x + ent.radius, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t ey = fclamp(ent.y + ent.radius, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    for (uint32_t x = sx; x <= ex; ++x)
        for (uint32_t y = sy; y <= ey; ++y)
            cells[(size_t)x * grid_y_cap + y].push_back(ent.id);
}

void SpatialHash::collide(std::function<void(Simulation *, Entity &, Entity &)> on_collide) {
    std::unordered_set<uint32_t> seen_collisions;
    for (uint32_t x = 0; x < width; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            std::vector<EntityID> const &cell = cells[(size_t)x * grid_y_cap + y];
            for (uint32_t i = 0; i < cell.size(); ++i) {
                for (uint32_t j = i + 1; j < cell.size(); ++j) {
                    uint32_t comb_hash = _hash_two(cell[i], cell[j]);
                    if (seen_collisions.contains(comb_hash)) continue;
                    on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell[j]));
                    seen_collisions.insert(comb_hash);
                }
            }
        }
    }
}

void SpatialHash::query(float x, float y, float w, float h, std::function<void(Simulation *, Entity &)> cb) {
    std::unordered_set<EntityID::id_type> seen_entities;
    uint32_t sx = fclamp(x - w, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t sy = fclamp(y - h, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    uint32_t ex = fclamp(x + w, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t ey = fclamp(y + h, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    for (uint32_t _x = sx; _x <= ex; ++_x) {
        for (uint32_t _y = sy; _y <= ey; ++_y) {
            std::vector<EntityID> const &cell = cells[(size_t)_x * grid_y_cap + _y];
            for (uint32_t i = 0; i < cell.size(); ++i) {
                Entity &ent = simulation->get_ent(cell[i]);
                if (ent.x + ent.radius < x - w) continue;
                if (ent.x - ent.radius > x + w) continue;
                if (ent.y + ent.radius < y - h) continue;
                if (ent.y - ent.radius > y + h) continue;
                if (seen_entities.contains(cell[i].id)) continue;
                cb(simulation, ent);
                seen_entities.insert(cell[i].id);
            }
        }
    }
}
