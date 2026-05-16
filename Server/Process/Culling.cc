#include <Server/Process.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <string>

constexpr float CULL_EXTRA_RADIUS = 250;

void tick_culling_behavior(Simulation *sim, Entity &ent) {
    float fov = fclamp(ent.fov, BASE_FOV * 0.3, BASE_FOV);
    std::string const map_path = ent.map_path;
    sim->spatial_hash.query(ent.camera_x, ent.camera_y, 960 / fov + CULL_EXTRA_RADIUS, 540 / fov + CULL_EXTRA_RADIUS, [map_path](Simulation *, Entity &ent) {
        if (ent.map_path == map_path) BIT_UNSET(ent.flags, EntityFlags::kIsCulled);
    });
}
