#include <Server/EntityFunctions.hh>

#include <Server/PetalTracker.hh>
#include <Server/Spawn.hh>

#include <Shared/Entity.hh>
#include <Shared/Helpers.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/Vector.hh>

#include <algorithm>
#include <cstring>
#include <iostream>

// Promote `base_id` to its highest-rarity sibling whose rarity ≤
// target_rarity. Two petals are siblings iff PETAL_DATA[].name matches.
// Used to upgrade mob drops by the mob's per-spawn rarity tier (Spawn.cc):
// a Mythic-rolled bee drops `kMythicTringer` even though its authored
// drop list contains `kCommonStinger`, because both name == "Stinger".
// Falls back to base_id if no sibling at higher rarity exists (e.g. for
// flag petals like kAntennae that only have one tier).
static PetalID::T _upgrade_drop(PetalID::T base_id, uint8_t target_rarity) {
    if (base_id == PetalID::kNone || base_id >= PetalID::kNumPetals) return base_id;
    if (PETAL_DATA[base_id].rarity >= target_rarity) return base_id;
    char const *name = PETAL_DATA[base_id].name;
    PetalID::T best = base_id;
    uint8_t best_rarity = PETAL_DATA[base_id].rarity;
    for (PetalID::T id = 1; id < PetalID::kNumPetals; ++id) {
        if (id == base_id) continue;
        if (std::strcmp(PETAL_DATA[id].name, name) != 0) continue;
        uint8_t r = PETAL_DATA[id].rarity;
        if (r > target_rarity) continue;
        if (r > best_rarity) {
            best = id;
            best_rarity = r;
        }
    }
    return best;
}

static void _alloc_drops(Simulation *sim, std::vector<PetalID::T> &success_drops, float x, float y) {
    #ifdef DEBUG
    for (PetalID::T id : success_drops)
        assert(id != PetalID::kNone && id < PetalID::kNumPetals);
    #endif
    size_t count = success_drops.size();
    for (size_t i = count; i > 0; --i) {
        PetalID::T drop_id = success_drops[i - 1];
        if (PETAL_DATA[drop_id].rarity == RarityID::kUnique && PetalTracker::get_count(sim, drop_id) > 0) {
            success_drops[i] = success_drops[count - 1];
            --count;
            success_drops.pop_back();
            PetalTracker::remove_petal(sim, drop_id);
        }
    }
    DEBUG_ONLY(assert(success_drops.size() == count);)
    if (count > 1) {
        for (size_t i = 0; i < count; ++i) {
            Entity &drop = alloc_drop(sim, success_drops[i]);
            drop.set_x(x);
            drop.set_y(y);
            drop.velocity.unit_normal(i * 2 * M_PI / count).set_magnitude(25);
        }
    } else if (count == 1) {
        Entity &drop = alloc_drop(sim, success_drops[0]);
        drop.set_x(x);
        drop.set_y(y);
    }
}

static void _add_score(Simulation *sim, EntityID const killer_id, Entity const &target) {
    if (!sim->ent_exists(killer_id)) return;
    Entity &killer = sim->get_ent(killer_id);
    if (killer.has_component(kScore))
        killer.set_score(killer.score + target.score_reward);
    if (target.has_component(kFlower) && sim->ent_alive(target.parent)) {
        Entity &camera = sim->get_ent(target.parent);
        if (!killer.has_component(kName)) camera.set_killed_by("");
        else camera.set_killed_by(killer.name);
    }
}

void entity_on_death(Simulation *sim, Entity const &ent) {
    //don't do on_death for any despawned entity
    uint8_t natural_despawn = BIT_AT(ent.flags, EntityFlags::kIsDespawning) && ent.despawn_tick == 0;
    if (ent.score_reward > 0 && sim->ent_exists(ent.last_damaged_by) && !natural_despawn) {
        EntityID killer_id = sim->get_ent(ent.last_damaged_by).base_entity;
        _add_score(sim, killer_id, ent);
    }
    if (ent.has_component(kMob)) {
        //if (!(ent.team == NULL_ENTITY)) return;
        if (BIT_AT(ent.flags, EntityFlags::kSpawnedFromZone))
            Map::remove_mob(sim, ent.zone);
        if (!natural_despawn && !(BIT_AT(ent.flags, EntityFlags::kNoDrops))) {
            struct MobData const &mob_data = MOB_DATA[ent.mob_id];
            std::vector<PetalID::T> success_drops = {};
            StaticArray<float, MAX_DROPS_PER_MOB> const &drop_chances = MOB_DROP_CHANCES[ent.mob_id];

            // Drop model: a single outcome per kill drawn from the
            // distribution { no_drop, drop_0, drop_1, ... } where
            // P(drop_i) = drop_chances[i] and P(no_drop) = 1 - Σchances.
            // For delta > 0 (mob rolled higher than authored) the entire
            // distribution shifts upward by `delta` tiers:
            //   - "no_drop" becomes a drop at tier `delta`
            //   - each authored drop's rarity becomes (authored + delta),
            //     saturating at Unique
            // Net effect: higher-rarity mobs drop at a higher rate AND at
            // higher rarities; a Mythic-rolled Common-authored mob (delta=5)
            // is guaranteed to drop at tier ≥ Mythic.
            int delta = (int)ent.mob_rarity - (int)mob_data.rarity;
            if (delta < 0) delta = 0;
            if (mob_data.drops.size() > 0) {
                float total = 0.0f;
                for (uint32_t i = 0; i < mob_data.drops.size(); ++i) total += drop_chances[i];
                float no_drop_mass = 1.0f - total;
                // Saturation: when the per-entry chances (each pre-clamped
                // to ≤ 1 in StaticData.cc) sum to > 1, the mob always
                // drops something. Re-normalise so the cumulative-sum
                // roll picks each entry in proportion to its chance;
                // without this, the first entry whose chance saturates at
                // 1.0 short-circuits the roll and is always picked (e.g.
                // Shiny Ladybug always producing Rare Dahlia and nothing
                // else).
                float norm = 1.0f;
                if (no_drop_mass < 0) {
                    no_drop_mass = 0;
                    norm = 1.0f / total;
                }
                float roll = frand();
                if (roll < no_drop_mass) {
                    if (delta > 0) {
                        // Pick the highest-base-chance entry as the carrier
                        // petal — its name determines which family the
                        // upgrade lookup walks.
                        uint32_t best = 0;
                        for (uint32_t i = 1; i < mob_data.drops.size(); ++i)
                            if (drop_chances[i] > drop_chances[best]) best = i;
                        uint32_t r = (uint32_t)delta;
                        if (r >= RarityID::kNumRarities) r = RarityID::kNumRarities - 1;
                        success_drops.push_back(_upgrade_drop(mob_data.drops[best], (uint8_t)r));
                    }
                    // else: original "no drop" outcome preserved for delta=0.
                } else {
                    float cum = no_drop_mass;
                    for (uint32_t i = 0; i < mob_data.drops.size(); ++i) {
                        cum += drop_chances[i] * norm;
                        if (roll < cum) {
                            uint32_t r = (uint32_t)PETAL_DATA[mob_data.drops[i]].rarity + (uint32_t)delta;
                            if (r >= RarityID::kNumRarities) r = RarityID::kNumRarities - 1;
                            success_drops.push_back(_upgrade_drop(mob_data.drops[i], (uint8_t)r));
                            break;
                        }
                    }
                }
            }
            _alloc_drops(sim, success_drops, ent.x, ent.y);
        }
        if (ent.mob_id == MobID::kAntHole && ent.team == NULL_ENTITY && frand() < DIGGER_SPAWN_CHANCE) {
            EntityID team = NULL_ENTITY;
            if (sim->ent_exists(ent.last_damaged_by))
                team = sim->get_ent(ent.last_damaged_by).team;
            // Inherit the Ant Hole's rolled rarity — a Mythic Hole drops
            // a Mythic-tier Digger.
            alloc_mob(sim, MobID::kDigger, ent.x, ent.y, team, (int)ent.mob_rarity);
        }

    } else if (ent.has_component(kPetal)) {
        if (ent.petal_id == PetalID::kWeb || ent.petal_id == PetalID::kTriweb)
            alloc_web(sim, 100, ent);
    } else if (ent.has_component(kFlower)) {
        std::vector<PetalID::T> potential = {};
        for (uint32_t i = 0; i < ent.loadout_count + MAX_SLOT_COUNT; ++i) {
            DEBUG_ONLY(assert(ent.loadout_ids[i] < PetalID::kNumPetals));
            PetalTracker::remove_petal(sim, ent.loadout_ids[i]);
            if (ent.loadout_ids[i] != PetalID::kNone && ent.loadout_ids[i] != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.loadout_ids[i]);
        }
        for (uint32_t i = 0; i < ent.deleted_petals.size(); ++i) {
            DEBUG_ONLY(assert(ent.deleted_petals[i] < PetalID::kNumPetals));
            PetalTracker::remove_petal(sim, ent.deleted_petals[i]);
            if (ent.deleted_petals[i] != PetalID::kNone && ent.deleted_petals[i] != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.deleted_petals[i]);
        }
        //no need to deleted_petals.clear, the player dies
        std::sort(potential.begin(), potential.end(), [](PetalID::T a, PetalID::T b) {
            return PETAL_DATA[a].rarity < PETAL_DATA[b].rarity;
        });

        std::vector<PetalID::T> success_drops = {};
        uint32_t numDrops = potential.size();
        if (numDrops > 3)
            numDrops = 3;
        for (uint32_t i = 0; i < numDrops; ++i) {
            PetalID::T p_id = potential.back();
            if (PETAL_DATA[p_id].rarity >= RarityID::kRare && frand() < 0.05) p_id = PetalID::kPollen;
            success_drops.push_back(p_id);
            potential.pop_back();
        }
        _alloc_drops(sim, success_drops, ent.x, ent.y);
        //if the camera is the one that disconnects
        //no need to re-add the petals to the petal tracker
        if (!sim->ent_alive(ent.parent))
            return;
        Entity &camera = sim->get_ent(ent.parent);
        //reset all reloads and stuff
        uint32_t num_left = potential.size();
        //set respawn level
        uint32_t respawn_level = div_round_up(3 * score_to_level(ent.score), 4);
        if (respawn_level > MAX_LEVEL) respawn_level = MAX_LEVEL;
        camera.set_respawn_level(respawn_level);
        uint32_t max_possible = MAX_SLOT_COUNT + loadout_slots_at_level(respawn_level);
        if (num_left > max_possible) num_left = max_possible;
        //fill petals
        for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
            camera.set_inventory(i, PetalID::kNone); //force reset
        for (uint32_t i = 0; i < num_left; ++i) {
            DEBUG_ONLY(assert(potential.back() < PetalID::kNumPetals));
            PetalTracker::add_petal(sim, potential.back());
            camera.set_inventory(i, potential.back());
            potential.pop_back();
        }
        //only track up to max_possible
        for (uint32_t i = num_left; i < max_possible; ++i)
            camera.set_inventory(i, PetalID::kNone); //don't track kNone
        //fill with basics
        for (uint32_t i = num_left; i < loadout_slots_at_level(respawn_level); ++i) {
            PetalTracker::add_petal(sim, PetalID::kBasic);
            camera.set_inventory(i, PetalID::kBasic);
        }
    } else if (ent.has_component(kDrop)) {
        if (BIT_AT(ent.flags, EntityFlags::kIsDespawning))
            PetalTracker::remove_petal(sim, ent.drop_id);
    }
}