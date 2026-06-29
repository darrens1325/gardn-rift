#include <Server/Process.hh>

#include <Shared/Entity.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

void tick_camera_behavior(Simulation *sim, Entity &ent) {
    if (sim->ent_exists(ent.player)) {
        Entity &player = sim->get_ent(ent.player);
        ent.set_camera_x(player.x);
        ent.set_camera_y(player.y);
        uint8_t max_rarity = 0;
        for (uint32_t i = 0; i < player.loadout_count + MAX_SLOT_COUNT; ++i) {
            PetalID::T id = player.loadout_ids[i];
            if (id == PetalID::kNone) continue;
            if (id.rarity > max_rarity) max_rarity = id.rarity;
        }
        player.set_loadout_count(loadout_slots_for_max_rarity(max_rarity));
        ent.last_damaged_by = player.last_damaged_by;
        struct ZoneDefinition const &zone = MAP[Map::get_zone_from_pos(player.x, player.y)];
        // if (zone.difficulty < Map::difficulty_at_level(score_to_level(player.score))) {
        //     if (player.overlevel_timer < PETAL_DISABLE_DELAY * SIM_RATE)
        //         player.set_overlevel_timer(player.overlevel_timer + 1);
        //     else player.set_overlevel_timer(PETAL_DISABLE_DELAY * SIM_RATE);
        // } else {
        //     if (player.overlevel_timer > 0)
        //         player.set_overlevel_timer(player.overlevel_timer - 0.1);
        //     else player.set_overlevel_timer(0);
        // }
    } else if (BIT_AT(ent.flags, EntityFlags::kIsSpectator)) {
        // Spectator camera (no owned player): follow the highest-scoring
        // live player on the same map so there's always action on screen
        // — e.g. watching bots while training runs under GARDN_SYNC. When
        // the arena is empty the camera just holds its last position.
        ent.set_fov(BASE_FOV * 0.9);
        Entity const *lead = nullptr;
        sim->for_each<kCamera>([&](Simulation *s, Entity &cam){
            if (!s->ent_alive(cam.player)) return;
            Entity const &p = s->get_ent(cam.player);
            if (p.map_path != ent.map_path) return;
            if (lead == nullptr || p.score > lead->score) lead = &p;
        });
        if (lead != nullptr) {
            ent.set_camera_x(lead->x);
            ent.set_camera_y(lead->y);
        }
    } else {
        ent.set_fov(BASE_FOV * 0.9);
        if (sim->ent_exists(ent.last_damaged_by)){
            Entity &viewer = sim->get_ent(ent.last_damaged_by);
            ent.set_camera_x(viewer.x);
            ent.set_camera_y(viewer.y);
        }
    }
}
