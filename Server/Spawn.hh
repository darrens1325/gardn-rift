#pragma once

#include <Shared/Entity.hh>

#include <string>

class Simulation;

Entity &alloc_drop(Simulation *, PetalID::T);
// `forced_rarity >= 0` overrides the per-spawn rarity roll for the new
// mob — used so that mob-spawn-mob sites (Queen Ant launching soldiers,
// Ant Hole spawning ants, Ant Hole death → Digger) propagate the parent
// mob's rarity tier into the children, keeping their size and stats
// consistent with the parent. -1 (default) keeps the existing behavior:
// roll uniformly in [authored, current_wave_rarity].
Entity &alloc_mob(Simulation *, MobID::T, float, float, EntityID const, int forced_rarity = -1);
Entity &alloc_mob_on_map(Simulation *, std::string const &, MobID::T, float, float, EntityID const, int forced_rarity = -1);
Entity &alloc_player(Simulation *, EntityID const);
Entity &alloc_petal(Simulation *, PetalID::T, Entity const &);
Entity &alloc_web(Simulation *, float, Entity const &);

// Wave-system per-rarity scale tables, exposed so AI code (Ai.cc) can
// resize projectiles fired by high-rarity mobs — a Mythic Hornet's
// missile and a Mythic Spider's web grow with the mob's body. The
// radius lookup mirrors MOB_RADIUS_MULT in Spawn.cc; the damage helper
// is the same 1.5^Δ curve we apply to mob.damage in __alloc_mob.
float mob_radius_mult(uint8_t rarity);
float mob_dmg_mult(uint8_t rolled_rarity, uint8_t authored_rarity);

void player_spawn(Simulation *, Entity &, Entity &);
