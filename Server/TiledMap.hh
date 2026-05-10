#pragma once

#include <Shared/StaticDefinitions.hh>

#include <cstdint>
#include <string>
#include <vector>

class Simulation;

struct TiledCollisionRect {
    float x, y, w, h;
};

struct TiledSpawnEntry {
    MobID::T id;
    float chance;
};

struct TiledSpawnPolygon {
    std::vector<float> vx;
    std::vector<float> vy;
    float min_x, min_y, max_x, max_y;
    float area;
    float density;
    std::vector<TiledSpawnEntry> spawns;
};

namespace TiledMap {
    extern bool loaded;
    extern std::vector<TiledCollisionRect> collision_rects;
    extern std::vector<TiledSpawnPolygon> spawn_polygons;

    // Loads the Tiled JSON map. Returns true on success.
    bool load(std::string const &path);

    bool point_in_polygon(TiledSpawnPolygon const &poly, float x, float y);

    // If (x, y) lies inside any collision rect, push the position
    // out along the shallowest axis. Operates on the entity's center;
    // padding by `radius` keeps the entity body outside the rect.
    void resolve_collision(float &x, float &y, float radius);

    // Spawn one random mob using the Tiled polygons (density-weighted).
    // Returns true if a spawn was attempted (whether it succeeded or not).
    bool spawn_random_mob(Simulation *sim);

    void note_mob_death(uint32_t poly_idx);
}
