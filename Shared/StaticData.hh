#pragma once

#include <Shared/StaticDefinitions.hh>

#include <array>
#include <cstdint>

extern uint32_t const MAX_LEVEL;
// Wall-clock tick rate. Configurable via -DTPS=N at build time. Bumping this
// makes the server schedule more `tick()` calls per real-second — useful for
// bot training. It is *not* the rate gameplay timing is calibrated against.
extern uint32_t const TPS;
// Game-time tick rate. ALL gameplay constants (AI durations, petal reloads,
// despawn timers, per-tick accelerations, spawn-rate frand probabilities) are
// calibrated against this. Fixed at 20 to preserve original game balance. Use
// SIM_RATE — not TPS — for anything that should be the same per game-second
// regardless of build-time TPS. The wall-clock speedup factor is TPS/SIM_RATE.
extern uint32_t const SIM_RATE;

extern float const PETAL_DISABLE_DELAY;
extern float const PLAYER_ACCELERATION;
extern float const DEFAULT_FRICTION;
extern float const SUMMON_RETREAT_RADIUS;
extern float const DIGGER_SPAWN_CHANCE;

extern float const BASE_FLOWER_RADIUS;
extern float const BASE_PETAL_ROTATION_SPEED;
extern float const BASE_FOV;
extern float const BASE_HEALTH;
extern float const BASE_BODY_DAMAGE;

extern struct PetalData const PETAL_DATA[PetalID::kNumPetals];
extern struct MobData const MOB_DATA[MobID::kNumMobs];

//map extends from (0,0) to (ARENA_WIDTH,ARENA_HEIGHT)
std::array const MAP = std::to_array<struct ZoneDefinition>({
    {
        .left = 0,
        .top = 0,
        .right = 10000,
        .bottom = 10000,
        .density = 1,
        .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kRock, 200000 },
            { MobID::kSpider, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kLadybug, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kBabyAnt, 25000 },
            { MobID::kCentipede, 10000 },
            { MobID::kBoulder, 10000 },
            { MobID::kMassiveLadybug, 200 },
            { MobID::kSquare, 1 },
            { MobID::kSoldierAnt, 50000}
        },
        .difficulty = 3,
        .color = 0xff1ea761,
        .name = "Easy"
    },
    {
        .left = 10000,
        .top = 0,
        .right = 20000,
        .bottom = 10000,
        .density = 1,
        .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kCactus, 400000 },
            { MobID::kBeetle, 100000 },
            { MobID::kSandstorm, 50000 },
            { MobID::kBee, 50000 },
            { MobID::kScorpion, 50000 },
            { MobID::kLadybug, 50000 },
            { MobID::kDesertCentipede, 10000 },
            { MobID::kAntHole, 2000 },
            { MobID::kShinyLadybug, 1000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3,
        .color = 0xffdecf7c,
        .name = "Medium"
    },
    {
        .left = 10000,
        .top = 10000,
        .right = 20000,
        .bottom = 20000,
        .density = 1,
        .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kSpider, 100000 },
            { MobID::kBoulder, 100000 },
            { MobID::kBee, 100000 },
            { MobID::kHornet, 100000 },
            { MobID::kBeetle, 50000 },
            { MobID::kLadybug, 50000 },
            { MobID::kCentipede, 10000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kMassiveBeetle, 2000 },
            { MobID::kAntHole, 2000 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3,
        .color = 0xffb06655,
        .name = "Hard"
    },
    {
        .left = 0,
        .top = 10000,
        .right = 10000,
        .bottom = 20000,
        .density = 1,
        .drop_multiplier = 0.3,
        .spawns = {
            { MobID::kDarkLadybug, 150000 },
            { MobID::kBeetle, 150000 },
            { MobID::kHornet, 150000 },
            { MobID::kSpider, 150000 },
            { MobID::kBoulder, 100000 },
            { MobID::kEvilCentipede, 10000 },
            { MobID::kMassiveBeetle, 2500 },
            { MobID::kAntHole, 2500 },
            { MobID::kSquare, 1 }
        },
        .difficulty = 3,
        .color = 0xff777777,
        .name = "???"
    }
});

std::array const ANTHOLE_SPAWNS = std::to_array<StaticArray<MobID::T, 3>>({
    {MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kBabyAnt},
    {MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt},
    {MobID::kBabyAnt,MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kWorkerAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kWorkerAnt,MobID::kWorkerAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kQueenAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt},
    {MobID::kSoldierAnt,MobID::kSoldierAnt,MobID::kSoldierAnt}
});

extern std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES;

extern uint32_t score_to_pass_level(uint32_t);
extern uint32_t score_to_level(uint32_t);
extern uint32_t level_to_score(uint32_t);
extern uint32_t loadout_slots_at_level(uint32_t);

extern float hp_at_level(uint32_t);