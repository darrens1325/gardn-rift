#include <Server/SpatialHash.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>

#include <algorithm>

SpatialHash::SpatialHash(Simulation *sim)
    : simulation(sim), grid_x_cap(0), grid_y_cap(0), width(1), height(1) {}

void SpatialHash::refresh(uint32_t _width, uint32_t _height) {
    DEBUG_ONLY(assert(_width <= ARENA_WIDTH && _height <= ARENA_HEIGHT));
    width = div_round_up(_width, GRID_SIZE);
    height = div_round_up(_height, GRID_SIZE);
    if (width > grid_x_cap || height > grid_y_cap) {
        // Grow + reallocate. Indexing depends on grid_y_cap, so growing
        // y also invalidates existing entries — but since we'd be about
        // to clear them anyway, just reset the whole backing store.
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
    //for the uniform grid to work, the max ent radius is GRID_SIZE/2
    //if larger entities are needed, either increase the GRID_SIZE
    //or use SpatialHashCanonical
    DEBUG_ONLY(assert(ent.radius <= GRID_SIZE / 2);)
    uint32_t x = fclamp(ent.x, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t y = fclamp(ent.y, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    cells[(size_t)x * grid_y_cap + y].push_back(ent.id);
}

void SpatialHash::collide(std::function<void(Simulation *, Entity &, Entity &)> on_collide) {
    for (uint32_t x = 0; x < width; ++x) {
        for (uint32_t y = 0; y < height; ++y) {
            std::vector<EntityID> &cell = cells[(size_t)x * grid_y_cap + y];
            for (uint32_t i = 0; i < cell.size(); ++i) {
                for (uint32_t j = i + 1; j < cell.size(); ++j) on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell[j]));
                if (x < width - 1) {
                    std::vector<EntityID> &cell2 = cells[(size_t)(x+1) * grid_y_cap + y];
                    for (uint32_t j = 0; j < cell2.size(); ++j) on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell2[j]));
                    if (y > 0) {
                        std::vector<EntityID> &cell2 = cells[(size_t)(x+1) * grid_y_cap + (y-1)];
                        for (uint32_t j = 0; j < cell2.size(); ++j) on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell2[j]));
                    }
                    if (y < height - 1) {
                        std::vector<EntityID> &cell2 = cells[(size_t)(x+1) * grid_y_cap + (y+1)];
                        for (uint32_t j = 0; j < cell2.size(); ++j) on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell2[j]));
                    }
                }
                if (y < height - 1) {
                    std::vector<EntityID> &cell2 = cells[(size_t)x * grid_y_cap + (y+1)];
                    for (uint32_t j = 0; j < cell2.size(); ++j) on_collide(simulation, simulation->get_ent(cell[i]), simulation->get_ent(cell2[j]));
                }
            }
        }
    }
}

void SpatialHash::query(float x, float y, float w, float h, std::function<void(Simulation *, Entity &)> cb) {
    uint32_t sx = fclamp(x - w - GRID_SIZE / 2, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t sy = fclamp(y - h - GRID_SIZE / 2, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    uint32_t ex = fclamp(x + w + GRID_SIZE / 2, 0, ARENA_WIDTH - 1) / GRID_SIZE;
    uint32_t ey = fclamp(y + h + GRID_SIZE / 2, 0, ARENA_HEIGHT - 1) / GRID_SIZE;
    for (uint32_t _x = sx; _x <= ex; ++_x) {
        for (uint32_t _y = sy; _y <= ey; ++_y) {
            std::vector<EntityID> &cell = cells[(size_t)_x * grid_y_cap + _y];
            for (uint32_t i = 0; i < cell.size(); ++i) {
                Entity &ent = simulation->get_ent(cell[i]);
                if (ent.x + ent.radius < x - w) continue;
                if (ent.x - ent.radius > x + w) continue;
                if (ent.y + ent.radius < y - h) continue;
                if (ent.y - ent.radius > y + h) continue;
                cb(simulation, ent);
            }
        }
    }
}
