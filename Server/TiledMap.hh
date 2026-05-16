#pragma once

#include <Shared/StaticDefinitions.hh>

#include <cstdint>
#include <string>
#include <vector>

class Simulation;

struct TiledCollisionRect {
    float x, y, w, h;
};

// Vertex of a per-tile collision polygon, in world-space (after the tile's
// flip transform + grid translation have been baked in at load time).
struct TiledPolyVert { float x, y; };

// A per-cell solid-tile polygon. Stored once per occupied solid cell;
// `verts` already contains the closed polygon in world coordinates so the
// collision check is just a circle-vs-polygon test without further math.
struct TiledCollisionPoly {
    std::vector<TiledPolyVert> verts;
    float min_x, min_y, max_x, max_y; // bounding box for early-out
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
    // Optional fixed-rarity override. When set, mobs spawned in this
    // polygon ignore the wave ramp and instead derive their rarity from
    // `difficulty` linearly: -2.5 → Common, 75 → Unique. Non-integer
    // positions blend the two adjacent rarities proportionally.
    bool has_difficulty = false;
    float difficulty = 0.f;
    std::vector<TiledSpawnEntry> spawns;
};

struct TiledWarp {
    std::string name;
    std::string map_path;
    std::string warp_point;
    float x, y, radius;
};

namespace TiledMap {
    extern bool loaded;
    extern std::string current_map_path;
    extern std::vector<TiledCollisionRect> collision_rects;
    extern std::vector<TiledCollisionPoly> collision_polys;
    extern std::vector<TiledSpawnPolygon> spawn_polygons;
    extern std::vector<TiledWarp> warps;

    // Loads the Tiled JSON map. Returns true on success.
    bool load(std::string const &path);

    bool ensure_loaded(std::string const &path);
    std::string default_map_path();
    uint32_t arena_width(std::string const &path);
    uint32_t arena_height(std::string const &path);

    bool point_in_polygon(TiledSpawnPolygon const &poly, float x, float y);

    // If (x, y) lies inside any collision rect, push the position
    // out along the shallowest axis. Operates on the entity's center;
    // padding by `radius` keeps the entity body outside the rect.
    void resolve_collision(float &x, float &y, float radius);
    void resolve_collision(std::string const &map_path, float &x, float &y, float radius);

    // True iff a straight line from (x0,y0) to (x1,y1) is interrupted by
    // any collision geometry. Polygons are approximated by their AABB —
    // exact enough for AI sight checks, cheap enough to run per target
    // candidate. Used to prevent mobs from aggroing through walls.
    bool line_of_sight_blocked(float x0, float y0, float x1, float y1);
    bool line_of_sight_blocked(std::string const &map_path, float x0, float y0, float x1, float y1);

    // Spawn one random mob using the Tiled polygons (density-weighted).
    // Returns true if a spawn was attempted (whether it succeeded or not).
    bool spawn_random_mob(Simulation *sim);

    void apply_warps(Simulation *sim);

    void note_mob_death(uint32_t poly_idx);
    void note_mob_death(std::string const &map_path, uint32_t poly_idx);
}
