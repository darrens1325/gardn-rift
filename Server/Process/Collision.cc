#include <Server/EntityFunctions.hh>

#include <Shared/Simulation.hh>
#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <cmath>
#include <iostream>

static bool _should_interact(Entity const &ent1, Entity const &ent2) {
    //if (ent1.has_component(kFlower) || ent2.has_component(kFlower)) return false;
    //if (ent1.has_component(kPetal) || ent2.has_component(kPetal)) return false;
    if (ent1.pending_delete || ent2.pending_delete) return false;
    if (ent1.map_path != ent2.map_path) return false;
    if (!(ent1.team == ent2.team)) return true;
    if (BIT_AT((ent1.flags | ent2.flags), EntityFlags::kNoFriendlyCollision)) return false;
    //if (ent1.has_component(kPetal) || ent2.has_component(kPetal)) return false;
    if (ent1.has_component(kMob) && ent2.has_component(kMob)) return true;
    return false;
}

static void _pickup_drop(Simulation *sim, Entity &player, Entity &drop) {
    if (!sim->ent_alive(player.parent)) return;
    if (drop.immunity_ticks > 0) return;

    for (uint32_t i = 0; i <  player.loadout_count + MAX_SLOT_COUNT; ++i) {
        if (player.loadout_ids[i] != PetalID::kNone) continue;
        player.set_loadout_ids(i, drop.drop_id);
        drop.set_x(player.x);
        drop.set_y(player.y);
        BIT_UNSET(drop.flags, EntityFlags::kIsDespawning);
        sim->request_delete(drop.id);
        //peaceful transfer, no petal tracking needed
        return;
    }
}

#define NO(component) (!ent1.has_component(component) && !ent2.has_component(component))
#define BOTH(component) (ent1.has_component(component) && ent2.has_component(component))
#define EITHER(component) (ent1.has_component(component) || ent2.has_component(component))

static void _deal_push(Entity &ent, Vector knockback, float mass_ratio, float scale) {
    if (fabsf(mass_ratio) < 0.01) return;
    knockback *= scale * mass_ratio;
    ent.collision_velocity += knockback;
}

static void _deal_knockback(Entity &ent, Vector knockback, float mass_ratio) {
    if (fabsf(mass_ratio) < 0.01) return;
    float scale = PLAYER_ACCELERATION * 2;
    knockback *= scale * mass_ratio;
    ent.collision_velocity += knockback;
    ent.velocity += knockback * 2;
}

static void _cancel_movement(Entity &ent, Vector dir, Vector add) {
    Vector push = dir;
    push.normalize();
    float dot = fclamp(push.x * add.x + push.y * add.y, PLAYER_ACCELERATION * 0.5, PLAYER_ACCELERATION * 25);
    ent.velocity += push * (PLAYER_ACCELERATION + dot * 2);
    ent.collision_velocity += push * (0.5 * PLAYER_ACCELERATION);
}

// A leech's body segments share one HP pool on the head; resolve any segment
// to the head so damage and lifesteal act on the single creature.
static Entity &_damage_target(Simulation *sim, Entity &e) {
    Entity *h = &e;
    for (int g = 0; g < 64 && h->has_component(kMob) && h->mob_id == MobID::kLeech
                 && h->is_tail && sim->ent_alive(h->seg_head); ++g)
        h = &sim->get_ent(h->seg_head);
    return *h;
}

// Contact damage from `atk` to `def`, plus any ocean on-hit effects keyed on
// the attacker (Claw bonus, Lightning chain, Fang/Leech lifesteal).
static void _attack(Simulation *sim, Entity &atk, Entity &def_seg) {
    // NOTE: do not bail on atk.health here. on_collide already verified both
    // entities were alive before either side struck; re-checking the attacker
    // would drop a petal's blow whenever the mob's hit broke it the same frame
    // (a contact trade is meant to be mutual).
    Entity &def = _damage_target(sim, def_seg);  // leech body -> shared head
    struct PetalAttributes const *pa = atk.has_component(kPetal)
        ? &PETAL_DATA[atk.petal_id.type][atk.petal_id.rarity].attributes : nullptr;

    float amt = atk.damage;
    // Claw: bonus damage versus targets still above 80% health.
    if (pa && pa->claw_bonus > 0 && def.max_health > 0 && def.health > 0.8f * def.max_health) {
        float bonus = pa->claw_bonus / 100.0f * def.max_health;
        if (bonus > pa->claw_limit) bonus = pa->claw_limit;
        amt += bonus;
    }

    float before = def.health;
    inflict_damage(sim, atk.id, def.id, amt, DamageType::kContact);
    float dealt = before - def.health;

    // Lightning petal: chain to nearby enemies (rate-limited via secondary_reload).
    if (pa && pa->lightning > 0 && atk.secondary_reload == 0) {
        chain_lightning(sim, atk, def_seg.id, pa->lightning, pa->lightning_bounces);
        atk.secondary_reload = SIM_RATE / 2;
    }
    // Fang petal: heal the owning flower for a fraction of damage dealt.
    if (pa && pa->lifesteal > 0 && dealt > 0 && sim->ent_alive(atk.parent)) {
        Entity &owner = sim->get_ent(atk.parent);
        if (owner.has_component(kHealth)) inflict_heal(sim, owner, dealt * pa->lifesteal / 100.0f);
    }
    // Leech mob: heal the shared head when any segment damages a flower.
    if (atk.has_component(kMob) && def_seg.has_component(kFlower) && dealt > 0) {
        struct MobAttributes const &ma = MOB_DATA[atk.mob_id].attributes;
        if (ma.lifesteal > 0) inflict_heal(sim, _damage_target(sim, atk), ma.lifesteal);
    }
}

void on_collide(Simulation *sim, Entity &ent1, Entity &ent2) {
    //do a distance dependent check first (it's faster)
    float min_dist = ent1.radius + ent2.radius;
    if (fabs(ent1.x - ent2.x) > min_dist || fabs(ent1.y - ent2.y) > min_dist) return;
    //check if collide (distance independent)
    if (!_should_interact(ent1, ent2)) return;
    //finer distance check
    Vector separation(ent1.x - ent2.x, ent1.y - ent2.y);
    float dist = min_dist - separation.magnitude();
    if (dist < 0) return;
    if (NO(kDrop) && NO(kWeb)) {
        if (separation.x == 0 && separation.y == 0)
            separation.unit_normal(frand() * 2 * M_PI);
        else
            separation.normalize();
        float ratio = ent2.mass / (ent1.mass + ent2.mass);
        if (!(ent1.team == ent2.team)) {
            if (ent1.has_component(kFlower) && !ent2.has_component(kPetal))
                _cancel_movement(ent1, separation, ent2.velocity - ent1.velocity);
            else
                _deal_knockback(ent1, separation, ratio);
            if (ent2.has_component(kFlower) && !ent1.has_component(kPetal))
                _cancel_movement(ent2, separation*-1, ent1.velocity - ent2.velocity);
            else
                _deal_knockback(ent2, separation*-1, 1 - ratio);
        }
        _deal_push(ent1, separation, ratio, dist);
        _deal_push(ent2, separation*-1, 1 - ratio, dist);
    }

    if (BOTH(kHealth) && !(ent1.team == ent2.team)) {
        if (ent1.health > 0 && ent2.health > 0) {
            _attack(sim, ent1, ent2);
            _attack(sim, ent2, ent1);
        }
    }

    if (ent1.has_component(kDrop) && ent2.has_component(kFlower)) 
        _pickup_drop(sim, ent2, ent1);
    if (ent2.has_component(kDrop) && ent1.has_component(kFlower))
        _pickup_drop(sim, ent1, ent2);

    if (ent1.has_component(kWeb) && !ent2.has_component(kPetal) && !ent2.has_component(kDrop))
        ent2.speed_ratio = 0.5;
    if (ent2.has_component(kWeb) && !ent1.has_component(kPetal) && !ent1.has_component(kDrop))
        ent1.speed_ratio = 0.5;
}
