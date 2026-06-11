#include <Server/EntityFunctions.hh>

#include <Server/Spawn.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/Helpers.hh>
#include <Shared/Vector.hh>

#include <cmath>

static bool _flower_has_sponge(Entity const &flower) {
    for (uint32_t i = 0; i < flower.loadout_count; ++i)
        if (flower.loadout_ids[i].type == PetalType::kSponge) return true;
    return false;
}

// The flower's first live Cotton petal, if any.
static EntityID _find_cotton(Simulation *sim, Entity &flower) {
    for (uint32_t i = 0; i < flower.loadout_count; ++i) {
        LoadoutSlot &slot = flower.loadout[i];
        if (slot.get_petal_id().type != PetalType::kCotton) continue;
        for (uint32_t j = 0; j < slot.size(); ++j)
            if (sim->ent_alive(slot.petals[j].ent_id)) return slot.petals[j].ent_id;
    }
    return NULL_ENTITY;
}

void inflict_damage(Simulation *sim, EntityID const atk_id, EntityID const def_id, float amt, uint8_t type) {
    if (amt <= 0) return;
    if (!sim->ent_alive(def_id)) return;
    Entity &defender = sim->get_ent(def_id);
    // The leech is a single creature whose body segments form its hitbox: any
    // damage to a body segment is dealt to the head, which holds the shared HP.
    if (defender.has_component(kMob) && defender.mob_id == MobID::kLeech && defender.is_tail) {
        Entity *h = &defender;
        for (int g = 0; g < 64 && h->is_tail && sim->ent_alive(h->seg_head); ++g)
            h = &sim->get_ent(h->seg_head);
        if (!(h->id == def_id)) { inflict_damage(sim, atk_id, h->id, amt, type); return; }
    }
    // Cotton: diverts incoming flower damage to the equipped Cotton petal,
    // which takes the hit in the flower's place. Only damage beyond the
    // cotton's current HP overflows back to the flower. Absorbs every damage
    // type (contact, lightning, poison, even Sponge's returned damage).
    if (defender.has_component(kFlower) && defender.immunity_ticks == 0) {
        EntityID cid = _find_cotton(sim, defender);
        if (sim->ent_alive(cid)) {
            float cotton_hp = sim->get_ent(cid).health;
            float absorbed = amt < cotton_hp ? amt : cotton_hp;
            if (absorbed > 0) inflict_damage(sim, atk_id, cid, absorbed, type);
            amt -= absorbed;
            if (amt <= 0) return;
        }
    }
    // Sponge: a flower carrying a Sponge absorbs incoming (non-DoT) damage and
    // bleeds it back gradually rather than all at once, modelled as a DoT.
    if (type != DamageType::kPoison && defender.has_component(kFlower)
        && defender.immunity_ticks == 0 && _flower_has_sponge(defender)) {
        float const SPONGE_RETURN_SECONDS = 2.5f;
        game_tick_t const ticks = (game_tick_t)(SPONGE_RETURN_SECONDS * SIM_RATE);
        defender.poison_inflicted += amt / ticks;
        if (defender.poison_ticks < ticks) defender.poison_ticks = ticks;
        defender.poison_dealer = atk_id;  // poison tick assigns kill credit
        defender.set_damaged(1);
        return;
    }
    if (!defender.has_component(kHealth)) return;
    DEBUG_ONLY(assert(!defender.pending_delete);)
    DEBUG_ONLY(assert(defender.has_component(kHealth));)
    if (defender.immunity_ticks > 0) return;
    if (type == DamageType::kContact) {
        if (defender.has_component(kFlower) && defender.armor_stacks > 0)
            amt -= defender.armor_per_stack;
        else
            amt -= defender.armor;
    }
    else if (type == DamageType::kPoison) amt -= defender.poison_armor;
    if (defender.has_component(kMob) && defender.mob_id == MobID::kLeafbug) {
        amt = fclamp(amt - 10, 0, amt);
    }
    if (type == DamageType::kContact && defender.has_component(kFlower) && defender.armor_stacks > 0) {
        --defender.armor_stacks;
        defender.armor = fclamp(defender.armor - defender.armor_per_stack, 0, defender.armor);
    }
    if (amt <= 0) return;
    //if (amt <= defender.armor) return;
    float old_health = defender.health;
    defender.set_damaged(1);
    defender.health = fclamp(defender.health - amt, 0, defender.health);  
    float damage_dealt = old_health - defender.health;
    //ant hole spawns
    //floor start, ceil end
    uint32_t const num_spawn_waves = ANTHOLE_SPAWNS.size() - 1;
    if (defender.has_component(kMob) && defender.mob_id == MobID::kAntHole) {
        uint32_t start = (old_health / defender.max_health) * num_spawn_waves;
        uint32_t end = ceilf((defender.health / defender.max_health) * num_spawn_waves);
        for (uint32_t i = start; i + 1 > end; --i) {
            for (MobID::T mob_id : ANTHOLE_SPAWNS[num_spawn_waves - i]) {
                // Damage-spawned ants inherit the Ant Hole's rolled
                // rarity so they match its size and tier.
                Entity &child = alloc_mob_on_map(sim, defender.map_path, mob_id, defender.x, defender.y, defender.team, (int)defender.mob_rarity);
                child.set_parent(defender.id);
                child.target = defender.target;
            }
        }
    }

    if (!sim->ent_exists(atk_id)) return;
    Entity &attacker = sim->get_ent(atk_id);

    if (type != DamageType::kReflect && defender.damage_reflection > 0)
        inflict_damage(sim, def_id, attacker.base_entity, damage_dealt * defender.damage_reflection, DamageType::kReflect);
    
    if (!sim->ent_alive(atk_id)) return;

    if (type == DamageType::kContact && defender.poison_ticks < attacker.poison_damage.time * SIM_RATE) {
        defender.poison_ticks = attacker.poison_damage.time * SIM_RATE;
        defender.poison_inflicted = attacker.poison_damage.damage / SIM_RATE;
        defender.poison_dealer = atk_id;
    }

    if (defender.slow_ticks < attacker.slow_inflict)
        defender.slow_ticks = attacker.slow_inflict;
    
    if (defender.has_component(kPetal)) {
        if (defender.petal_id.type == PetalType::kDandelion)
            attacker.dandy_ticks = 10 * SIM_RATE;
    }

    if (attacker.has_component(kPetal)) {
        if (!sim->ent_alive(defender.target))
            defender.target = attacker.parent;
        defender.last_damaged_by = attacker.parent;
    } else {
        if (!sim->ent_alive(defender.target))
            defender.target = atk_id;
        defender.last_damaged_by = atk_id;
    }
}

void inflict_heal(Simulation *sim, Entity &ent, float amt) {
    DEBUG_ONLY(assert(ent.has_component(kHealth));)
    if (ent.pending_delete || ent.health <= 0) return;
    if (ent.dandy_ticks > 0) return;
    ent.health = fclamp(ent.health + amt, 0, ent.max_health);
}

// Chain lightning, ported from ~/flooooio. Deals `damage` to `first_target`,
// then arcs to the nearest not-yet-struck enemy of `source`'s team, repeating
// until `max_hits` entities have been hit or no target remains in range.
// Used by the Jellyfish mob and the Lightning petal.
void chain_lightning(Simulation *sim, Entity &source, EntityID first_target, float damage, uint32_t max_hits) {
    if (damage <= 0 || max_hits == 0) return;
    float const BOUNCE_RADIUS = 350.0f;
    EntityID hit_ids[32];
    uint32_t hit_count = 0;
    EntityID current = first_target;
    while (hit_count < max_hits && hit_count < 32 && sim->ent_alive(current)) {
        Entity &cur = sim->get_ent(current);
        float cx = cur.x, cy = cur.y;
        inflict_damage(sim, source.id, current, damage, DamageType::kContact);
        hit_ids[hit_count++] = current;

        EntityID next = NULL_ENTITY;
        float min_dist = BOUNCE_RADIUS;
        sim->spatial_hash.query(cx, cy, BOUNCE_RADIUS, BOUNCE_RADIUS, [&](Simulation *s, Entity &e){
            if (!s->ent_alive(e.id)) return;
            if (e.map_path != source.map_path) return;
            if (e.team == source.team) return;
            if (e.immunity_ticks > 0) return;
            if (!e.has_component(kMob) && !e.has_component(kFlower)) return;
            for (uint32_t i = 0; i < hit_count; ++i) if (e.id == hit_ids[i]) return;
            float d = Vector(e.x - cx, e.y - cy).magnitude();
            if (d >= min_dist) return;
            min_dist = d; next = e.id;
        });
        current = next;
    }
}
