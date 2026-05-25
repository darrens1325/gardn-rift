#include <Server/EntityFunctions.hh>
#include <Server/TiledMap.hh>

#include <Server/PetalTracker.hh>
#include <Server/Spawn.hh>

#include <Shared/Entity.hh>
#include <Shared/Helpers.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/Vector.hh>

#include <algorithm>
#include <iostream>

// Promote `base_id` to its highest-rarity sibling whose rarity ≤
// target_rarity. Siblings share `PetalType` — under the 2D table that's
// just "walk the rarity axis of the same column". Used to upgrade mob
// drops by the mob's per-spawn rarity tier (Spawn.cc): a Mythic-rolled
// bee drops the Mythic-tier Stinger family member even though its
// authored drop list contains the Common Stinger. Falls back to base_id
// if no higher-rarity sibling cell is authored (PETAL_DATA[t][r].name ==
// nullptr marks an unauthored cell).
static PetalID::T _upgrade_drop(PetalID::T base_id, uint8_t target_rarity) {
    if (base_id == PetalID::kNone) return base_id;
    if (base_id.rarity >= target_rarity) return base_id;
    PetalID::T best = base_id;
    for (uint8_t r = base_id.rarity + 1; r <= target_rarity && r < RarityID::kNumRarities; ++r) {
        if (PETAL_DATA[base_id.type][r].name == nullptr) continue;
        best = { base_id.type, r };
    }
    return best;
}

static void _alloc_drops(Simulation *sim, std::vector<PetalID::T> &success_drops, std::string const &map_path, float x, float y) {
    #ifdef DEBUG
    for (PetalID::T id : success_drops)
        assert(id != PetalID::kNone);
    #endif
    size_t count = success_drops.size();
    for (size_t i = count; i > 0; --i) {
        PetalID::T drop_id = success_drops[i - 1];
        if (drop_id.rarity == RarityID::kUnique && PetalTracker::get_count(sim, drop_id) > 0) {
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
            drop.map_path = map_path;
            drop.set_x(x);
            drop.set_y(y);
            drop.velocity.unit_normal(i * 2 * M_PI / count).set_magnitude(25);
        }
    } else if (count == 1) {
        Entity &drop = alloc_drop(sim, success_drops[0]);
        drop.map_path = map_path;
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
            TiledMap::note_mob_death(ent.map_path, ent.zone);
        if (!natural_despawn && !(BIT_AT(ent.flags, EntityFlags::kNoDrops))) {
            struct MobData const &mob_data = MOB_DATA[ent.mob_id];
            std::vector<PetalID::T> success_drops = {};
            // BR maps look up the per-(mob, view_rarity) row from
            // MOB_DROP_CHANCES_BR (3D table). The view rarity is the
            // entity's runtime rolled rarity, so a Mythic-rolled Bee uses
            // the gallery's "Bee Mythic" distribution directly — no delta
            // upgrade is needed because the per-view chances already
            // express what florr's gallery shows at each rarity.
            //
            // NORMAL maps stay on the legacy delta-upgrade model: chances
            // are stored at the authored rarity and runtime-rolled
            // higher-rarity mobs upgrade their drops by `delta` tiers.
            bool const is_br = ent.map_path.rfind("Map/br/", 0) == 0;
            uint8_t view = ent.mob_rarity;
            if (view >= RarityID::kNumRarities) view = RarityID::kNumRarities - 1;
            StaticArray<float, MAX_DROPS_PER_MOB> const &drop_chances =
                is_br ? MOB_DROP_CHANCES_BR[ent.mob_id][view]
                      : MOB_DROP_CHANCES_NORMAL[ent.mob_id];

            // Drop model: one independent roll per *petal type* in the
            // mob's authored drop list. A mob can drop at most one petal
            // of any given type, but it can drop multiple petals of the
            // same rarity (across different types). Within a type, the
            // authored rarity entries form a distribution: the rarity
            // actually dropped is sampled in proportion to its chance.
            //
            // delta upgrade only applies on NORMAL maps; BR uses the
            // per-view chances directly.
            int delta = is_br ? 0 : (int)ent.mob_rarity - (int)mob_data.rarity;
            if (delta < 0) delta = 0;
            uint32_t const r_cap = RarityID::kNumRarities - 1;

            // Group indices into mob_data.drops by petal type, in the
            // order types first appear. Iteration order in the resulting
            // vector matches authored order for deterministic rolls.
            std::vector<PetalType::T> type_order;
            std::vector<std::vector<uint32_t>> indices_by_type;
            for (uint32_t i = 0; i < mob_data.drops.size(); ++i) {
                PetalType::T t = mob_data.drops[i].type;
                auto it = std::find(type_order.begin(), type_order.end(), t);
                if (it == type_order.end()) {
                    type_order.push_back(t);
                    indices_by_type.push_back({i});
                } else {
                    indices_by_type[it - type_order.begin()].push_back(i);
                }
            }

            for (size_t k = 0; k < type_order.size(); ++k) {
                std::vector<uint32_t> const &idxs = indices_by_type[k];
                float total = 0.0f;
                for (uint32_t i : idxs) total += drop_chances[i];
                float no_drop_mass = 1.0f - total;
                // Same saturation rescue as the legacy single-outcome
                // model: if the entries for this type sum to >1 we treat
                // the type as guaranteed-drop and rescale within-type
                // weights to pick the rarity in proportion to its chance.
                float norm = 1.0f;
                if (no_drop_mass < 0) {
                    no_drop_mass = 0;
                    norm = 1.0f / total;
                }
                float roll = frand();
                if (roll < no_drop_mass) {
                    if (delta > 0) {
                        // Carrier within the type: highest-chance entry.
                        uint32_t best = idxs[0];
                        for (uint32_t i : idxs) if (drop_chances[i] > drop_chances[best]) best = i;
                        uint32_t r = (uint32_t)delta;
                        if (r > r_cap) r = r_cap;
                        success_drops.push_back(_upgrade_drop(mob_data.drops[best], (uint8_t)r));
                    }
                    // else: no drop of this type for this kill.
                } else {
                    float cum = no_drop_mass;
                    for (uint32_t i : idxs) {
                        cum += drop_chances[i] * norm;
                        if (roll < cum) {
                            uint32_t r = (uint32_t)mob_data.drops[i].rarity + (uint32_t)delta;
                            if (r > r_cap) r = r_cap;
                            success_drops.push_back(_upgrade_drop(mob_data.drops[i], (uint8_t)r));
                            break;
                        }
                    }
                }
            }
            _alloc_drops(sim, success_drops, ent.map_path, ent.x, ent.y);
        }
        if (ent.mob_id == MobID::kAntHole && ent.team == NULL_ENTITY && frand() < DIGGER_SPAWN_CHANCE) {
            EntityID team = NULL_ENTITY;
            if (sim->ent_exists(ent.last_damaged_by))
                team = sim->get_ent(ent.last_damaged_by).team;
            // Inherit the Ant Hole's rolled rarity — a Mythic Hole drops
            // a Mythic-tier Digger.
            alloc_mob_on_map(sim, ent.map_path, MobID::kDigger, ent.x, ent.y, team, (int)ent.mob_rarity);
        }

    } else if (ent.has_component(kPetal)) {
        if (ent.petal_id == PetalID::kWeb || ent.petal_id == PetalID::kTriweb)
            alloc_web(sim, 100, ent);
    } else if (ent.has_component(kFlower)) {
        std::vector<PetalID::T> potential = {};
        for (uint32_t i = 0; i < ent.loadout_count + MAX_SLOT_COUNT; ++i) {
            PetalTracker::remove_petal(sim, ent.loadout_ids[i]);
            if (ent.loadout_ids[i] != PetalID::kNone && ent.loadout_ids[i] != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.loadout_ids[i]);
        }
        for (uint32_t i = 0; i < ent.deleted_petals.size(); ++i) {
            PetalTracker::remove_petal(sim, ent.deleted_petals[i]);
            if (ent.deleted_petals[i] != PetalID::kNone && ent.deleted_petals[i] != PetalID::kBasic && frand() < 0.95)
                potential.push_back(ent.deleted_petals[i]);
        }
        //no need to deleted_petals.clear, the player dies
        std::sort(potential.begin(), potential.end(), [](PetalID::T a, PetalID::T b) {
            return a.rarity < b.rarity;
        });

        std::vector<PetalID::T> success_drops = {};
        uint32_t numDrops = potential.size();
        if (numDrops > 3)
            numDrops = 3;
        for (uint32_t i = 0; i < numDrops; ++i) {
            PetalID::T p_id = potential.back();
            if (p_id.rarity >= RarityID::kRare && frand() < 0.05) p_id = PetalID::kPollen;
            success_drops.push_back(p_id);
            potential.pop_back();
        }
        _alloc_drops(sim, success_drops, ent.map_path, ent.x, ent.y);
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
