#include <Server/Spawn.hh>

#include <Server/EntityFunctions.hh>
#include <Server/PetalTracker.hh>
#include <Server/Server.hh>
#include <Server/TiledMap.hh>
#include <Shared/Helpers.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/StaticDefinitions.hh>

#include <cmath>

static std::string const *g_alloc_mob_map_path = nullptr;

Entity &alloc_drop(Simulation *sim, PetalID::T drop_id) {
    DEBUG_ONLY(assert(drop_id < PetalID::kNumPetals);)
    PetalTracker::add_petal(sim, drop_id);
    Entity &drop = sim->alloc_ent();
    drop.add_component(kPhysics);
    drop.set_radius(25);
    drop.set_angle(frand() * 0.2 - 0.1);
    drop.friction = 0.25;

    drop.add_component(kRelations);
    drop.set_team(NULL_ENTITY);

    drop.add_component(kDrop);
    drop.set_drop_id(drop_id);
    entity_set_despawn_tick(drop, 10 * (2 + PETAL_DATA[drop_id].rarity) * SIM_RATE);
    drop.immunity_ticks = SIM_RATE / 3;
    return drop;
}

// Wave-rarity scaling for mob stats. The mob's *effective* rarity at spawn
// time is max(authored, current_wave_rarity); we never downgrade an Epic-
// authored mob into a Common one.
//
// HP / damage / xp scale by `delta = effective - authored` so authored-Epic
// mobs stay tougher than authored-Common ones at the same wave tier. HP
// uses the steepest curve (1.7× per tier) so the bot feels the round-end
// difficulty wall; damage scales more gently so late-round mobs aren't
// flat-out unkillable.
//
// Radius is *absolute* by effective rarity rather than delta-based: a
// Mythic mob always reads as twice the size of a Common one regardless of
// how it was authored. Per-rarity multipliers per the spec — linear +0.2
// per tier from Common (1.0×) through Mythic (2.0×); Unique extrapolates
// to 2.2× to keep the curve monotonic.
static constexpr float _mob_scale_pow(float base, int n) {
    float r = 1.0f;
    if (n >= 0) for (int i = 0; i < n; ++i) r *= base;
    else       for (int i = 0; i < -n; ++i) r /= base;
    return r;
}
static constexpr float MOB_RADIUS_MULT[RarityID::kNumRarities] = {
    1.0f,  // Common
    1.1f,  // Unusual
    1.3f,  // Rare
    1.72f,  // Epic
    3.0f,  // Legendary
    5.0f,  // Mythic
    7.0f,  // Unique  (extrapolation of the +0.2/tier ramp)
};

float mob_radius_mult(uint8_t rarity) {
    if (rarity >= RarityID::kNumRarities) rarity = RarityID::kNumRarities - 1;
    return MOB_RADIUS_MULT[rarity];
}
float mob_dmg_mult(uint8_t rolled_rarity, uint8_t authored_rarity) {
    if (rolled_rarity >= RarityID::kNumRarities) rolled_rarity = RarityID::kNumRarities - 1;
    int delta = (int)rolled_rarity - (int)authored_rarity;
    if (delta < 0) delta = 0;
    return _mob_scale_pow(1.5f, delta);
}

// Pick a per-spawn rarity for a mob: floor at the authored rarity, ceiling
// at the current wave_rarity, uniform within. Returned via out-parameter so
// the segmented-mob path can roll *once* for the whole snake and feed the
// result into every segment via __alloc_mob's `forced_rarity` arg.
static uint32_t _roll_mob_rarity(Simulation *sim, MobID::T mob_id) {
    struct MobData const &data = MOB_DATA[mob_id];
    uint32_t wave = sim->current_wave_rarity;
    if (wave >= RarityID::kNumRarities) wave = RarityID::kNumRarities - 1;
    uint32_t lo = data.rarity;
    uint32_t hi = wave > lo ? wave : lo;
    uint32_t span = hi - lo + 1;
    uint32_t rolled = lo + (uint32_t)(frand() * span);
    if (rolled > hi) rolled = hi;
    return rolled;
}

static Entity &__alloc_mob(Simulation *sim, MobID::T mob_id, float x, float y, EntityID const team = NULL_ENTITY, int forced_rarity = -1) {
    DEBUG_ONLY(assert(mob_id < MobID::kNumMobs);)
    struct MobData const &data = MOB_DATA[mob_id];
    // Per-spawn rarity. Wave is the ceiling, authored rarity is the floor;
    // uniform within. A Mythic-wave bee field has a mix of Common, Unusual,
    // ..., Mythic bees side by side rather than every bee being Mythic.
    // Higher-rarity rolls hit higher HP / damage / xp / radius multipliers
    // (below) and a higher drop-chance multiplier (Death.cc), so mythic
    // bees are visibly more dangerous and more worth killing than the
    // commons in the same wave. `forced_rarity >= 0` overrides the roll,
    // used by the segmented path so all segments share one tier.
    uint32_t rolled = (forced_rarity >= 0) ? (uint32_t)forced_rarity : _roll_mob_rarity(sim, mob_id);
    int delta = (int)rolled - (int)data.rarity;
    float hp_mult     = _mob_scale_pow(1.7f, delta);
    float dmg_mult    = _mob_scale_pow(1.5f, delta);
    float xp_mult     = _mob_scale_pow(1.6f, delta);
    float radius_mult = MOB_RADIUS_MULT[rolled];
    float seed = frand();
    Entity &mob = sim->alloc_ent();
    mob.map_path = g_alloc_mob_map_path ? *g_alloc_mob_map_path : TiledMap::default_map_path();

    mob.add_component(kPhysics);
    mob.set_radius(data.radius.get_single(seed) * radius_mult);
    mob.set_angle(frand() * 2 * M_PI);
    mob.set_x(x);
    mob.set_y(y);
    mob.friction = DEFAULT_FRICTION;
    mob.mass = (1 + mob.radius / BASE_FLOWER_RADIUS) * (data.attributes.stationary ? 10000 : 1);
    if (mob_id == MobID::kAntHole)
        BIT_SET(mob.flags, EntityFlags::kNoFriendlyCollision);
    if (team == NULL_ENTITY)
        BIT_SET(mob.flags, EntityFlags::kHasCulling);

    mob.add_component(kRelations);
    mob.set_team(team);

    mob.add_component(kMob);
    mob.set_mob_id(mob_id);
    mob.set_mob_rarity((uint8_t)rolled);

    mob.add_component(kHealth);
    mob.health = mob.max_health = data.health.get_single(seed) * hp_mult;
    mob.damage = data.damage * dmg_mult;
    mob.poison_damage = data.attributes.poison_damage;
    mob.poison_damage.damage *= dmg_mult;
    mob.set_health_ratio(1);

    mob.detection_radius = data.attributes.aggro_radius;
    mob.score_reward = (uint32_t)(data.xp * xp_mult);

    mob.add_component(kName);
    mob.set_name(data.name);

    mob.base_entity = mob.id;
    if (mob_id == MobID::kDigger) {
        mob.add_component(kFlower);
        mob.set_angle(0);
        mob.set_color(ColorID::kGray);
    }
    return mob;
}

Entity &alloc_mob(Simulation *sim, MobID::T mob_id, float x, float y, EntityID const team, int forced_rarity) {
    struct MobData const &data = MOB_DATA[mob_id];
    if (data.attributes.segments <= 1) {
        Entity &ent = __alloc_mob(sim, mob_id, x, y, team, forced_rarity);
        if (mob_id == MobID::kAntHole) {
            // The Ant Hole's initial ant burst inherits the hole's
            // rolled rarity — a Mythic Ant Hole pops out Mythic-tier
            // baby/worker/soldier ants matching its size.
            int child_rarity = (int)ent.mob_rarity;
            std::vector<MobID::T> const spawns = {
                MobID::kBabyAnt, MobID::kBabyAnt, MobID::kBabyAnt,
                MobID::kWorkerAnt, MobID::kWorkerAnt, MobID::kSoldierAnt
            };
            for (MobID::T mob_id : spawns) {
                Vector rand = Vector::rand(ent.radius * 2);
                Entity &ant = __alloc_mob(sim, mob_id, x + rand.x, y + rand.y, team, child_rarity);
                ant.set_parent(ent.id);
            }
        }
        return ent;
    }
    else {
        // Roll the rarity once for the whole snake — otherwise each segment
        // would roll independently and you'd see a centipede with a Common
        // head, a Mythic torso, and a Rare tail (each segment also picks
        // its own radius_mult, so the segments wouldn't even fit together).
        int shared_rarity = forced_rarity >= 0 ? forced_rarity : (int)_roll_mob_rarity(sim, mob_id);
        Entity &head = __alloc_mob(sim, mob_id, x, y, team, shared_rarity);
        head.add_component(kSegmented);
        head.set_is_tail(0);
        Entity *curr = &head;
        for (uint32_t i = 1; i < data.attributes.segments; ++i) {
            Entity &seg = __alloc_mob(sim, mob_id, x, y, team, shared_rarity);
            seg.add_component(kSegmented);
            seg.set_is_tail(1);
            seg.seg_head = curr->id;
            seg.set_angle(curr->angle + frand() * 0.1 - 0.05);
            seg.set_x(curr->x - (curr->radius + seg.radius) * cosf(seg.angle));
            seg.set_y(curr->y - (curr->radius + seg.radius) * sinf(seg.angle));
            curr = &seg;
        }
        return head;
    }
}

Entity &alloc_mob_on_map(Simulation *sim, std::string const &map_path, MobID::T mob_id, float x, float y, EntityID const team, int forced_rarity) {
    std::string const *prev = g_alloc_mob_map_path;
    g_alloc_mob_map_path = &map_path;
    Entity &ent = alloc_mob(sim, mob_id, x, y, team, forced_rarity);
    g_alloc_mob_map_path = prev;
    return ent;
}

Entity &alloc_player(Simulation *sim, EntityID const team) {
    Entity &player = sim->alloc_ent();
    player.map_path = TiledMap::default_map_path();

    player.add_component(kPhysics);
    player.set_radius(BASE_FLOWER_RADIUS);
    player.friction = DEFAULT_FRICTION;
    player.mass = 1;

    player.add_component(kFlower);

    player.add_component(kRelations);
    player.set_team(team);

    player.add_component(kHealth);
    player.health = player.max_health = BASE_HEALTH;
    player.set_health_ratio(1);
    player.damage = BASE_BODY_DAMAGE;
    player.immunity_ticks = 1.0 * SIM_RATE;

    player.add_component(kScore);

    player.add_component(kName);
    player.set_nametag_visible(1);

    player.base_entity = player.id;
    return player;
}

Entity &alloc_petal(Simulation *sim, PetalID::T petal_id, Entity const &parent) {
    DEBUG_ONLY(assert(petal_id < PetalID::kNumPetals);)
    struct PetalData const &petal_data = PETAL_DATA[petal_id];
    Entity &petal = sim->alloc_ent();
    petal.map_path = parent.map_path;
    petal.add_component(kPhysics);
    petal.set_x(parent.x);
    petal.set_y(parent.y);
    petal.set_radius(petal_data.radius);
    if (petal_data.attributes.rotation_style == PetalAttributes::kPassiveRot)
        petal.set_angle(frand() * 2 * M_PI);
    petal.mass = petal_data.attributes.mass;
    petal.friction = DEFAULT_FRICTION * 1.5;
    petal.add_component(kRelations);
    petal.set_parent(parent.id);
    petal.set_team(parent.team);
    petal.add_component(kPetal);
    petal.set_petal_id(petal_id);
    petal.add_component(kHealth);
    petal.health = petal.max_health = petal_data.health;
    petal.damage = petal_data.damage;
    petal.set_health_ratio(1);
    petal.poison_damage = petal_data.attributes.poison_damage;
    if (petal_id == PetalID::kPincer) petal.slow_inflict = SIM_RATE * 1.5;
    if (petal_id == PetalID::kBone) petal.armor = 4;

    if (parent.id == NULL_ENTITY) petal.base_entity = petal.id;
    else petal.base_entity = parent.id;
    return petal;
}

Entity &alloc_web(Simulation *sim, float radius, Entity const &parent) {
    Entity &web = sim->alloc_ent();
    web.map_path = parent.map_path;
    web.add_component(kPhysics);
    web.set_x(parent.x);
    web.set_y(parent.y);
    web.set_angle(frand() * 2 * M_PI);
    web.set_radius(radius);
    web.mass = 1.0;
    web.friction = 1.0;
    web.add_component(kRelations);
    web.set_team(parent.team);
    web.set_parent(parent.id);
    web.add_component(kWeb);
    entity_set_despawn_tick(web, 10 * SIM_RATE);
    return web;
}

void player_spawn(Simulation *sim, Entity &camera, Entity &player) {
    camera.set_player(player.id);
    player.set_parent(camera.id);
    player.set_color(camera.color);
    if (camera.map_path.empty()) camera.map_path = TiledMap::default_map_path();
    player.map_path = camera.map_path;
    uint32_t power = Map::difficulty_at_level(camera.respawn_level);
    // Retry the random spawn point against the Tiled map's collision
    // geometry — without this, spawn lands inside a wall ~10–20% of the
    // time (the map has 1500+ solid-cell polys plus a few hand-placed
    // collision rects). TiledMap::resolve_collision push-out is the
    // post-tick fallback, but spawning *visually* in the wall and only
    // then sliding out feels broken to players. Reject up to 30 picks
    // that need any push, falling back to the push-out path if every
    // sample lands in geometry (e.g. a degenerate map config).
    float spawn_x = 0.0f, spawn_y = 0.0f;
    for (int attempt = 0; attempt < 30; ++attempt) {
        uint32_t arena_width = TiledMap::arena_width(player.map_path);
        uint32_t arena_height = TiledMap::arena_height(player.map_path);
        spawn_x = lerp(arena_width * 0.1, arena_width * 0.9, frand());
        spawn_y = lerp(arena_height * 0.1, arena_height * 0.9, frand());
        float orig_x = spawn_x, orig_y = spawn_y;
        TiledMap::resolve_collision(player.map_path, spawn_x, spawn_y, BASE_FLOWER_RADIUS);
        // No push needed → position was free.
        if (std::fabs(spawn_x - orig_x) < 0.5f && std::fabs(spawn_y - orig_y) < 0.5f) break;
        // Otherwise fall through and re-roll. After exhausting attempts
        // we'll keep the last resolved (= pushed-to-edge) position.
    }
    camera.set_camera_x(spawn_x);
    camera.set_camera_y(spawn_y);
    player.set_x(spawn_x);
    player.set_y(spawn_y);
    player.set_score(level_to_score(camera.respawn_level));
    player.set_loadout_count(loadout_slots_at_level(camera.respawn_level));
    player.health = player.max_health = hp_at_level(camera.respawn_level);
    for (uint32_t i = 0; i < player.loadout_count; ++i) {
        PetalID::T id = camera.inventory[i];
        LoadoutSlot &slot = player.loadout[i];
        player.set_loadout_ids(i, id);
        slot.update_id(sim, id);
        slot.force_reload();
    }

    for (uint32_t i = player.loadout_count; i < player.loadout_count + MAX_SLOT_COUNT; ++i)
        player.set_loadout_ids(i, camera.inventory[i]);

    //peaceful transfer, no petal tracking needed
    for (uint32_t i = 0; i < MAX_SLOT_COUNT * 2; ++i)
        camera.set_inventory(i, PetalID::kNone);
}
