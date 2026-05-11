#include <Shared/StaticData.hh>

#include <Shared/Map.hh>

#include <cmath>

uint32_t const MAX_LEVEL = 99;
// Wall-clock tick rate. Override at compile time with -DGARDN_TPS=N (CMake
// flag `-DTPS=N`) — useful for accelerating bot training. Default 20 matches
// the original game pacing.
#ifndef GARDN_TPS
#define GARDN_TPS 20
#endif
uint32_t const TPS = GARDN_TPS;
// Game-time tick rate. Fixed at 20: changing this would rebalance every
// gameplay constant (reload, AI duration, etc.) all at once. See StaticData.hh.
uint32_t const SIM_RATE = 20;

float const PETAL_DISABLE_DELAY = 45.0f; //seconds
float const PLAYER_ACCELERATION = 5.0f;
float const DEFAULT_FRICTION = 1.0f/3.0f;
float const SUMMON_RETREAT_RADIUS = 600.0f;
float const DIGGER_SPAWN_CHANCE = 0.25f;

float const BASE_FLOWER_RADIUS = 25.0f;
float const BASE_PETAL_ROTATION_SPEED = 2.5f;
float const BASE_FOV = 0.9f;
float const BASE_HEALTH = 200.0f;
float const BASE_BODY_DAMAGE = 25.0f;

// -----------------------------------------------------------------------------
// Rarity scale helpers. Used by the wave-system rarity expansion at the bottom
// of PETAL_DATA: every "combat-relevant" base petal gets all 7 rarity tiers
// generated mechanically from one existing tier so we don't have to hand-tune
// 100+ stat tuples. Constants tuned to roughly match the existing hand-tuned
// progressions (e.g. kHeavy: 20 → 35 → 60 → 100 → 160 ≈ 1.6× per step on HP,
// 20 → 30 → 45 → 70 → 100 ≈ 1.5× per step on damage).
//
// Reload, count, radius, mass and the boolean / enum attribute fields are
// rarity-invariant — they're tactical knobs, not power knobs.
// -----------------------------------------------------------------------------
namespace {
constexpr float scale_pow(float base, int n) {
    float r = 1.0f;
    if (n >= 0) for (int i = 0; i < n; ++i) r *= base;
    else       for (int i = 0; i < -n; ++i) r /= base;
    return r;
}
}
constexpr float scale_hp(float base, int delta)     { return base * scale_pow(1.6f, delta); }
constexpr float scale_dmg(float base, int delta)    { return base * scale_pow(1.5f, delta); }
constexpr float scale_heal(float base, int delta)   { return base * scale_pow(1.5f, delta); }
constexpr float scale_poison(float base, int delta) { return base * scale_pow(1.5f, delta); }

struct PetalData const PETAL_DATA[PetalID::kNumPetals] = {
    {"None", "How can you see this?",
        0.0, 0.0, 0.0, 1.0, 0, RarityID::kCommon, {}},
    {"Basic", "A nice petal, not too strong but not too weak",
        10.0, 10.0, 10.0, 2.5, 1, RarityID::kCommon, {}},
    {"Basic", "A nice petal, not too strong but not too weak",
        18.0, 16.0, 10.0, 2.5, 1, RarityID::kUnusual, {}},
    {"Basic", "A nice petal, not too strong but not too weak",
        30.0, 25.0, 10.0, 2.5, 1, RarityID::kRare, {}},
    {"Basic", "A nice petal, not too strong but not too weak",
        50.0, 40.0, 10.0, 2.5, 1, RarityID::kEpic, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        5.0, 8.0, 7.0, 1.0, 1, RarityID::kCommon, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        8.0, 14.0, 7.0, 1.0, 1, RarityID::kUnusual, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        12.0, 22.0, 7.0, 1.0, 1, RarityID::kRare, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        20.0, 35.0, 7.0, 1.0, 1, RarityID::kEpic, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        20.0, 20.0, 12.0, 4.5, 1, RarityID::kCommon, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        35.0, 30.0, 12.0, 4.5, 1, RarityID::kUnusual, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        60.0, 45.0, 12.0, 4.5, 1, RarityID::kRare, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        100.0, 70.0, 12.0, 4.5, 1, RarityID::kEpic, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        160.0, 100.0, 12.0, 4.5, 1, RarityID::kLegendary, {}},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 20.0, 7.0, 3.5, 1, RarityID::kCommon, {}},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 35.0, 7.0, 3.5, 1, RarityID::kUnusual, {}},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 50.0, 7.0, 3.5, 1, RarityID::kRare, {}},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 75.0, 7.0, 3.5, 1, RarityID::kEpic, {}},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        6.0, 6.0, 10.0, 1.0, 1, RarityID::kCommon, {
        .constant_heal = 0.5,
        .icon_angle = -1
    }},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        10.0, 8.0, 10.0, 1.0, 1, RarityID::kUnusual, {
        .constant_heal = 1,
        .icon_angle = -1
    }},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        18.0, 12.0, 10.0, 1.0, 1, RarityID::kRare, {
        .constant_heal = 1.5,
        .icon_angle = -1
    }},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        30.0, 18.0, 10.0, 1.0, 1, RarityID::kEpic, {
        .constant_heal = 2.5,
        .icon_angle = -1
    }},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        50.0, 25.0, 10.0, 1.0, 1, RarityID::kLegendary, {
        .constant_heal = 4.0,
        .icon_angle = -1
    }},
    {"Twin", "Why stop at one? Why not TWO?!",
        5.0, 8.0, 7.0, 1.0, 2, RarityID::kUnusual, {}},
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        3.0, 3.0, 10.0, 3.5, 1, RarityID::kCommon, {
        .secondary_reload = 1.0,
        .burst_heal = 5,
        .defend_only = 1
    }},
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        5.0, 5.0, 10.0, 3.5, 1, RarityID::kUnusual, {
        .secondary_reload = 1.0,
        .burst_heal = 10,
        .defend_only = 1
    }},
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        7.0, 7.0, 10.0, 3.5, 1, RarityID::kRare, {
        .secondary_reload = 1.0,
        .burst_heal = 15,
        .defend_only = 1
    }},
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        10.0, 10.0, 10.0, 3.5, 1, RarityID::kLegendary, {
        .secondary_reload = 1.0,
        .burst_heal = 35,
        .defend_only = 1
    }},
    {"Iris", "Very poisonous, but takes a while to do its work",
        3.0, 3.0, 7.0, 5.0, 1, RarityID::kCommon, {
        .poison_damage = { 5.0, 6.0 }
    }},
    {"Iris", "Very poisonous, but takes a while to do its work",
         5.0, 5.0, 7.0, 5.0, 1, RarityID::kUnusual, {
        .poison_damage = { 10.0, 6.0 }
    }},
    {"Iris", "Very poisonous, but takes a while to do its work",
        7.0, 7.0, 7.0, 5.0, 1, RarityID::kRare, {
        .poison_damage = { 20.0, 6.0 }
    }},
    {"Iris", "Very poisonous, but takes a while to do its work",
        15.0, 15.0, 7.0, 5.0, 1, RarityID::kLegendary, {
        .poison_damage = { 40.0, 5.0 }
    }},
    {"Missile", "You can actually shoot this one",
        5.0, 25.0, 10.0, 1.0, 1, RarityID::kRare, {
        .secondary_reload = 0.5, 
        .defend_only = 1,
        .icon_angle = 1,
        .rotation_style = PetalAttributes::kFollowRot 
    }},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        10.0, 10.0, 10.0, 1.0, 1, RarityID::kRare, {
        .icon_angle = 1,
        .rotation_style = PetalAttributes::kFollowRot 
    }},
    {"Bubble", "You can right click to pop it and propel your flower",
        0.5, 0.0, 12.0, 3.0, 1, RarityID::kCommon, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Bubble", "You can right click to pop it and propel your flower",
        0.7, 0.0, 12.0, 2.5, 1, RarityID::kUnusual, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Bubble", "You can right click to pop it and propel your flower",
        1.0, 0.0, 12.0, 2.0, 1, RarityID::kRare, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Bubble", "You can right click to pop it and propel your flower",
        1.5, 0.0, 12.0, 1.5, 1, RarityID::kEpic, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Bubble", "You can right click to pop it and propel your flower",
        2.0, 0.0, 12.0, 1.0, 1, RarityID::kLegendary, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Faster", "It's so light it makes your other petals spin faster",
        5.0, 7.0, 7.0, 0.5, 1, RarityID::kRare, {}},
    {"Rock", "Even more durable, but slower to recharge",
        30.0, 5.0, 12.0, 7.5, 1, RarityID::kCommon, {}},
    {"Rock", "Even more durable, but slower to recharge",
        60.0, 7.0, 12.0, 7.5, 1, RarityID::kUnusual, {}},
    {"Rock", "Even more durable, but slower to recharge",
        100.0, 10.0, 12.0, 7.5, 1, RarityID::kRare, {}},
    {"Rock", "Even more durable, but slower to recharge",
        200.0, 15.0, 12.0, 7.5, 1, RarityID::kEpic, {}},
    {"Rock", "Even more durable, but slower to recharge",
        350.0, 25.0, 12.0, 7.5, 1, RarityID::kLegendary, {}},
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        15.0, 5.0, 15.0, 1.0, 1, RarityID::kRare, {}},
    {"Web", "It's really sticky",
        10.0, 5.0, 10.0, 3.0, 1, RarityID::kRare, {
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Wing", "It comes and goes",
        15.0, 15.0, 10.0, 2.5, 1, RarityID::kRare, {
        .icon_angle = 1,
    }},
    {"Peas", "4 in 1 deal",
        5.0, 8.0, 7.0, 2.0, 4, RarityID::kRare, {
        .clump_radius = 8,
        .secondary_reload = 0.1,
        .defend_only = 1,
    }},
    {"Sand", "It's coarse, rough, and gets everywhere",
        10.0, 3.0, 7.0, 1.5, 4, RarityID::kRare, {
        .clump_radius = 10,
    }},
    {"Pincer", "Stuns and poisons targets for a short duration",
        10.0, 5.0, 10.0, 2.5, 1, RarityID::kRare, {
        .icon_angle = 0.7,
        .poison_damage = { 5.0, 1.0 }
    }},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        5.0, 5.0, 7.0, 3.5, 3, RarityID::kRare, { 
        .clump_radius = 10,
        .secondary_reload = 1.0,
        .burst_heal = 3.5,
        .defend_only = 1
    }},
    {"Triplet", "How about THREE?!",
        5.0, 8.0, 7.0, 1.0, 3, RarityID::kEpic, {}},
    {"Egg", "Something interesting might pop out of this",
        50.0, 1.0, 12.5, 1.0, 2, RarityID::kEpic, { 
        .secondary_reload = 3.5,
        .defend_only = 1,
        .rotation_style = PetalAttributes::kNoRot,
        .spawns = MobID::kSoldierAnt
    }},
    {"Iris", "Deals its effects quicker than traditional irises",
        10.0, 5.0, 7.0, 5.0, 1, RarityID::kEpic, { 
        .poison_damage = { 15.0, 4.0 }
    }},
    {"Pollen", "Asthmatics beware",
        7.0, 8.0, 7.0, 1.5, 3, RarityID::kEpic, {
        .secondary_reload = 0.5,
        .defend_only = 1
    }},
    {"Peas", "4 in 1 deal, now with a secret ingredient: poison",
        5.0, 2.0, 7.0, 2.0, 4, RarityID::kEpic, {
        .clump_radius = 8,
        .secondary_reload = 0.1,
        .defend_only = 1,
        .poison_damage = { 20.0, 0.5 }
    }},
    {"Egg", "Something interesting might pop out of this",
        50.0, 1.0, 15.0, 1.0, 1, RarityID::kEpic, { 
        .secondary_reload = 3.5,
        .defend_only = 1,
        .rotation_style = PetalAttributes::kNoRot,
        .spawns = MobID::kBeetle
    }},
    {"Rose", "Extremely powerful rose, almost unheard of",
        5.0, 5.0, 10.0, 3.5, 1, RarityID::kEpic, { 
        .secondary_reload = 1.0,
        .burst_heal = 22,
        .defend_only = 1
    }},
    {"Stick", "Harnesses the power of the wind",
        10.0, 1.0, 15.0, 3.0, 1, RarityID::kLegendary, { 
        .secondary_reload = 4.0,
        .defend_only = 1,
        .icon_angle = 1,
        .spawns = MobID::kSandstorm,
        .spawn_count = 2
    }},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 35.0, 7.0, 4.5, 3, RarityID::kLegendary, {
        .clump_radius = 10
    }},
    {"Stinger", "It really hurts, but it's really fragile",
        5.0, 50.0, 7.0, 4.5, 3, RarityID::kMythic, {
        .clump_radius = 10
    }},
    {"Web", "It's really sticky",
        10.0, 5.0, 10.0, 3.0, 3, RarityID::kLegendary, {
        .clump_radius = 10,
        .secondary_reload = 0.5,
        .defend_only = 1,
    }},
    {"Antennae", "Allows your flower to sense foes from farther away",
        0.0, 0.0, 12.5, 0.0, 0, RarityID::kLegendary, {}},
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        15.0, 5.0, 10.0, 1.0, 3, RarityID::kLegendary, {
        .clump_radius = 10,
    }},
    {"Heaviest", "This thing is so heavy that nothing gets in the way",
        200.0, 10.0, 12.0, 15.0, 1, RarityID::kEpic, {
        .mass = 10,
        .rotation_style = PetalAttributes::kNoRot
    }},
    {"Third Eye", "Allows your flower to extend petals further out",
        0.0, 0.0, 20.0, 0.0, 0, RarityID::kMythic, {}},
    {"Observer", "The one who sees all", 
        0.0, 0.0, 12.5, 0.0, 0, RarityID::kMythic, {}},
    {"Cactus", "Turns your flower poisonous. Enemies will take poison damage on contact",
        15.0, 5.0, 10.0, 1.0, 1, RarityID::kEpic, {
        .poison_damage = { 1.0, 5.0 }
    }},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        10.0, 10.0, 10.0, 2.5, 1, RarityID::kRare, {}},
    {"Basic", "Something incredibly rare and useless",
        10.0, 10.0, 10.0, 2.5, 1, RarityID::kUnique, {}},
    {"Square", "This shape... it looks familiar...",
        10.0, 10.0, 15.0, 2.5, 1, RarityID::kUnique, {
        .icon_angle = M_PI / 4 + 1
    }},
    {"Moon", "Where did this come from?",
        1000.0, 1.0, 50.0, 10.0, 1, RarityID::kMythic, {
        .secondary_reload = 0.5,
        .mass = 200
    }},
    {"Lotus", "Absorbs some poison damage taken by the flower",
        5.0, 5.0, 12.0, 2.0, 1, RarityID::kEpic, {
        .icon_angle = 0.1
    }},
    {"Cutter", "Increases the flower's body damage",
        0.0, 0.0, 40.0, 0.0, 0, RarityID::kEpic, {}},
    {"Yin Yang", "Alters the flower's petal rotation in interesting ways",
        15.0, 15.0, 10.0, 2.5, 1, RarityID::kEpic, {}},
    {"Yggdrasil", "Unfortunately, its powers are useless here",
        1.0, 1.0, 12.0, 10.0, 1, RarityID::kUnique, {
        .defend_only = 1,
        .icon_angle = M_PI
    }},
    {"Rice", "Spawns instantly, but not very strong",
        1.0, 4.0, 13.0, 0.1, 1, RarityID::kEpic, {
        .icon_angle = 0.7
    }},
    {"Bone", "Sturdy",
        12.0, 10.0, 12.0, 2.5, 1, RarityID::kLegendary, {
        .icon_angle = 1
    }},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        10.0, 5.0, 10.0, 1.0, 1, RarityID::kUnusual, {
        .constant_heal = 1.5,
        .icon_angle = -1
    }},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        500.0, 2.5, 16.0, 10.0, 1, RarityID::kEpic, {
        .icon_angle = 0.5
    }},
    // -----------------------------------------------------------------------
    // Wave-system rarity expansion. Order MUST match the new PetalID enum
    // entries appended at the end of StaticDefinitions.hh::PetalID — these
    // rows are indexed by enum value. Stats are formula-scaled from the
    // nearest hand-tuned existing tier; the "// Δ from kX (rarity Y)" comment
    // on each block names the source entry so the math is auditable.
    // -----------------------------------------------------------------------
    // Basic — Δ from kEpicBasic (50/40 @ Epic=3)
    {"Basic", "A nice petal, not too strong but not too weak",
        scale_hp(50.0, 1), scale_dmg(40.0, 1), 10.0, 2.5, 1, RarityID::kLegendary, {}},
    {"Basic", "A nice petal, not too strong but not too weak",
        scale_hp(50.0, 2), scale_dmg(40.0, 2), 10.0, 2.5, 1, RarityID::kMythic, {}},
    // Fast (Light) — Δ from kEpicLight (20/35 @ Epic=3)
    {"Fast", "Weaker than most petals, but reloads very quickly",
        scale_hp(20.0, 1), scale_dmg(35.0, 1), 7.0, 1.0, 1, RarityID::kLegendary, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        scale_hp(20.0, 2), scale_dmg(35.0, 2), 7.0, 1.0, 1, RarityID::kMythic, {}},
    {"Fast", "Weaker than most petals, but reloads very quickly",
        scale_hp(20.0, 3), scale_dmg(35.0, 3), 7.0, 1.0, 1, RarityID::kUnique, {}},
    // Heavy — Δ from kLegendaryHeavy (160/100 @ Legendary=4)
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        scale_hp(160.0, 1), scale_dmg(100.0, 1), 12.0, 4.5, 1, RarityID::kMythic, {}},
    {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
        scale_hp(160.0, 2), scale_dmg(100.0, 2), 12.0, 4.5, 1, RarityID::kUnique, {}},
    // Stinger — Δ from kMythicTringer (5/50, count=3, clump=10 @ Mythic=5)
    {"Stinger", "It really hurts, but it's really fragile",
        scale_hp(5.0, 1), scale_dmg(50.0, 1), 7.0, 4.5, 3, RarityID::kUnique, {.clump_radius = 10}},
    // Leaf — Δ from kLegendaryLeaf (50/25, heal=4 @ Legendary=4)
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        scale_hp(50.0, 1), scale_dmg(25.0, 1), 10.0, 1.0, 1, RarityID::kMythic, {.constant_heal = scale_heal(4.0, 1), .icon_angle = -1}},
    {"Leaf", "Gathers energy from the sun to passively heal your flower",
        scale_hp(50.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1, RarityID::kUnique, {.constant_heal = scale_heal(4.0, 2), .icon_angle = -1}},
    // Twin — Δ from kTwin (5/8, count=2 @ Unusual=1)
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 1.0, 2, RarityID::kCommon, {}},
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 1.0, 2, RarityID::kRare, {}},
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 1.0, 2, RarityID::kEpic, {}},
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 2, RarityID::kLegendary, {}},
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 1.0, 2, RarityID::kMythic, {}},
    {"Twin", "Why stop at one? Why not TWO?!",
        scale_hp(5.0, 5), scale_dmg(8.0, 5), 7.0, 1.0, 2, RarityID::kUnique, {}},
    // Rose — Δ from kLegendaryRose (10/10, burst=35 @ Legendary=4)
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 3.5, 1, RarityID::kMythic,
        {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 1), .defend_only = 1}},
    {"Rose", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 3.5, 1, RarityID::kUnique,
        {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 2), .defend_only = 1}},
    // Iris — Δ from kLegendaryIris (15/15, poison=40/5 @ Legendary=4)
    {"Iris", "Very poisonous, but takes a while to do its work",
        scale_hp(15.0, 1), scale_dmg(15.0, 1), 7.0, 5.0, 1, RarityID::kMythic,
        {.poison_damage = {scale_poison(40.0, 1), 5.0}}},
    {"Iris", "Very poisonous, but takes a while to do its work",
        scale_hp(15.0, 2), scale_dmg(15.0, 2), 7.0, 5.0, 1, RarityID::kUnique,
        {.poison_damage = {scale_poison(40.0, 2), 5.0}}},
    // Bubble — Δ from kLegendaryBubble (2.0/0 @ Legendary=4)
    {"Bubble", "You can right click to pop it and propel your flower",
        scale_hp(2.0, 1), 0.0, 12.0, 1.0, 1, RarityID::kMythic,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Bubble", "You can right click to pop it and propel your flower",
        scale_hp(2.0, 2), 0.0, 12.0, 1.0, 1, RarityID::kUnique,
        {.secondary_reload = 0.5, .defend_only = 1}},
    // Rock — Δ from kLegendaryRock (350/25 @ Legendary=4)
    {"Rock", "Even more durable, but slower to recharge",
        scale_hp(350.0, 1), scale_dmg(25.0, 1), 12.0, 7.5, 1, RarityID::kMythic, {}},
    {"Rock", "Even more durable, but slower to recharge",
        scale_hp(350.0, 2), scale_dmg(25.0, 2), 12.0, 7.5, 1, RarityID::kUnique, {}},
    // Web — Δ from kWeb (10/5 @ Rare=2). Mythic/Unique mirror kTriweb's clump.
    {"Web", "It's really sticky",
        scale_hp(10.0, -2), scale_dmg(5.0, -2), 10.0, 3.0, 1, RarityID::kCommon,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Web", "It's really sticky",
        scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 3.0, 1, RarityID::kUnusual,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Web", "It's really sticky",
        scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 3.0, 1, RarityID::kEpic,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Web", "It's really sticky",
        scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 3.0, 3, RarityID::kMythic,
        {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
    {"Web", "It's really sticky",
        scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 3.0, 3, RarityID::kUnique,
        {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
    // Wing — Δ from kWing (15/15 @ Rare=2)
    {"Wing", "It comes and goes",
        scale_hp(15.0, -2), scale_dmg(15.0, -2), 10.0, 2.5, 1, RarityID::kCommon, {.icon_angle = 1}},
    {"Wing", "It comes and goes",
        scale_hp(15.0, -1), scale_dmg(15.0, -1), 10.0, 2.5, 1, RarityID::kUnusual, {.icon_angle = 1}},
    {"Wing", "It comes and goes",
        scale_hp(15.0, 1), scale_dmg(15.0, 1), 10.0, 2.5, 1, RarityID::kEpic, {.icon_angle = 1}},
    {"Wing", "It comes and goes",
        scale_hp(15.0, 2), scale_dmg(15.0, 2), 10.0, 2.5, 1, RarityID::kLegendary, {.icon_angle = 1}},
    {"Wing", "It comes and goes",
        scale_hp(15.0, 3), scale_dmg(15.0, 3), 10.0, 2.5, 1, RarityID::kMythic, {.icon_angle = 1}},
    {"Wing", "It comes and goes",
        scale_hp(15.0, 4), scale_dmg(15.0, 4), 10.0, 2.5, 1, RarityID::kUnique, {.icon_angle = 1}},
    // Peas — Δ from kPeas (5/8 count=4 @ Rare=2)
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, -2), scale_dmg(8.0, -2), 7.0, 2.0, 4, RarityID::kCommon,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 2.0, 4, RarityID::kUnusual,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 2.0, 4, RarityID::kLegendary,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 2.0, 4, RarityID::kMythic,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 2.0, 4, RarityID::kUnique,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    // Sand — Δ from kSand (10/3 count=4 @ Rare=2)
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, -2), scale_dmg(3.0, -2), 7.0, 1.5, 4, RarityID::kCommon, {.clump_radius = 10}},
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, -1), scale_dmg(3.0, -1), 7.0, 1.5, 4, RarityID::kUnusual, {.clump_radius = 10}},
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, 1), scale_dmg(3.0, 1), 7.0, 1.5, 4, RarityID::kEpic, {.clump_radius = 10}},
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, 2), scale_dmg(3.0, 2), 7.0, 1.5, 4, RarityID::kLegendary, {.clump_radius = 10}},
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, 3), scale_dmg(3.0, 3), 7.0, 1.5, 4, RarityID::kMythic, {.clump_radius = 10}},
    {"Sand", "It's coarse, rough, and gets everywhere",
        scale_hp(10.0, 4), scale_dmg(3.0, 4), 7.0, 1.5, 4, RarityID::kUnique, {.clump_radius = 10}},
    // Pincer — Δ from kPincer (10/5 + poison=5/1 @ Rare=2)
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, -2), scale_dmg(5.0, -2), 10.0, 2.5, 1, RarityID::kCommon,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, -2), 1.0}}},
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 2.5, 1, RarityID::kUnusual,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, -1), 1.0}}},
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 2.5, 1, RarityID::kEpic,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 1), 1.0}}},
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, 2), scale_dmg(5.0, 2), 10.0, 2.5, 1, RarityID::kLegendary,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 2), 1.0}}},
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 2.5, 1, RarityID::kMythic,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 3), 1.0}}},
    {"Pincer", "Stuns and poisons targets for a short duration",
        scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 2.5, 1, RarityID::kUnique,
        {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 4), 1.0}}},
    // Dahlia — Δ from kDahlia (5/5 count=3 burst=3.5 @ Rare=2)
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, -2), scale_dmg(5.0, -2), 7.0, 3.5, 3, RarityID::kCommon,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, -2), .defend_only = 1}},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, -1), scale_dmg(5.0, -1), 7.0, 3.5, 3, RarityID::kUnusual,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, -1), .defend_only = 1}},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, 1), scale_dmg(5.0, 1), 7.0, 3.5, 3, RarityID::kEpic,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 1), .defend_only = 1}},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, 2), scale_dmg(5.0, 2), 7.0, 3.5, 3, RarityID::kLegendary,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 2), .defend_only = 1}},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, 3), scale_dmg(5.0, 3), 7.0, 3.5, 3, RarityID::kMythic,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 3), .defend_only = 1}},
    {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
        scale_hp(5.0, 4), scale_dmg(5.0, 4), 7.0, 3.5, 3, RarityID::kUnique,
        {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 4), .defend_only = 1}},
    // Triplet — Δ from kTriplet (5/8 count=3 @ Epic=3)
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, -3), scale_dmg(8.0, -3), 7.0, 1.0, 3, RarityID::kCommon, {}},
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, -2), scale_dmg(8.0, -2), 7.0, 1.0, 3, RarityID::kUnusual, {}},
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 1.0, 3, RarityID::kRare, {}},
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 1.0, 3, RarityID::kLegendary, {}},
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 1.0, 3, RarityID::kMythic, {}},
    {"Triplet", "How about THREE?!",
        scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 3, RarityID::kUnique, {}},
    // Salt — Δ from kSalt (10/10 @ Rare=2)
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 2.5, 1, RarityID::kCommon, {}},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 2.5, 1, RarityID::kUnusual, {}},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 2.5, 1, RarityID::kEpic, {}},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 2.5, 1, RarityID::kLegendary, {}},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 2.5, 1, RarityID::kMythic, {}},
    {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
        scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 2.5, 1, RarityID::kUnique, {}},
    // Pollen — Δ from kPollen (7/8 count=3 @ Epic=3)
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, -3), scale_dmg(8.0, -3), 7.0, 1.5, 3, RarityID::kCommon,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, -2), scale_dmg(8.0, -2), 7.0, 1.5, 3, RarityID::kUnusual,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, -1), scale_dmg(8.0, -1), 7.0, 1.5, 3, RarityID::kRare,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, 1), scale_dmg(8.0, 1), 7.0, 1.5, 3, RarityID::kLegendary,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, 2), scale_dmg(8.0, 2), 7.0, 1.5, 3, RarityID::kMythic,
        {.secondary_reload = 0.5, .defend_only = 1}},
    {"Pollen", "Asthmatics beware",
        scale_hp(7.0, 3), scale_dmg(8.0, 3), 7.0, 1.5, 3, RarityID::kUnique,
        {.secondary_reload = 0.5, .defend_only = 1}},
    // Faster — Δ from kFaster (5/7 reload=0.5 @ Rare=2)
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, -2), scale_dmg(7.0, -2), 7.0, 0.5, 1, RarityID::kCommon, {}},
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, -1), scale_dmg(7.0, -1), 7.0, 0.5, 1, RarityID::kUnusual, {}},
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, 1), scale_dmg(7.0, 1), 7.0, 0.5, 1, RarityID::kEpic, {}},
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, 2), scale_dmg(7.0, 2), 7.0, 0.5, 1, RarityID::kLegendary, {}},
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, 3), scale_dmg(7.0, 3), 7.0, 0.5, 1, RarityID::kMythic, {}},
    {"Faster", "It's so light it makes your other petals spin faster",
        scale_hp(5.0, 4), scale_dmg(7.0, 4), 7.0, 0.5, 1, RarityID::kUnique, {}},
    // Cactus — Δ from kCactus (15/5 @ Rare=2). kPoisonCactus / kTricac
    // already cover Epic / Legendary so we only fill Common, Unusual,
    // Mythic, Unique.
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        scale_hp(15.0, -2), scale_dmg(5.0, -2), 15.0, 1.0, 1, RarityID::kCommon, {}},
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        scale_hp(15.0, -1), scale_dmg(5.0, -1), 15.0, 1.0, 1, RarityID::kUnusual, {}},
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        scale_hp(15.0, 3), scale_dmg(5.0, 3), 15.0, 1.0, 1, RarityID::kMythic, {}},
    {"Cactus", "Not very strong, but somehow increases your maximum health",
        scale_hp(15.0, 4), scale_dmg(5.0, 4), 15.0, 1.0, 1, RarityID::kUnique, {}},
    // Missile — Δ from kMissile (5/25 @ Rare=2)
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, -2), scale_dmg(25.0, -2), 10.0, 1.0, 1, RarityID::kCommon,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, -1), scale_dmg(25.0, -1), 10.0, 1.0, 1, RarityID::kUnusual,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, 1), scale_dmg(25.0, 1), 10.0, 1.0, 1, RarityID::kEpic,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1, RarityID::kLegendary,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, 3), scale_dmg(25.0, 3), 10.0, 1.0, 1, RarityID::kMythic,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Missile", "You can actually shoot this one",
        scale_hp(5.0, 4), scale_dmg(25.0, 4), 10.0, 1.0, 1, RarityID::kUnique,
        {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    // Dandelion — Δ from kDandelion (10/10 @ Rare=2)
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 1.0, 1, RarityID::kCommon,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 1.0, 1, RarityID::kUnusual,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 1.0, 1, RarityID::kEpic,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 1.0, 1, RarityID::kLegendary,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1, RarityID::kMythic,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    {"Dandelion", "Its interesting properties prevent healing effects on affected units",
        scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 1.0, 1, RarityID::kUnique,
        {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    // Yucca — Δ from kYucca (10/5 + heal=1.5 @ Unusual=1)
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 1.0, 1, RarityID::kCommon,
        {.constant_heal = scale_heal(1.5, -1), .icon_angle = -1}},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 1.0, 1, RarityID::kRare,
        {.constant_heal = scale_heal(1.5, 1), .icon_angle = -1}},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, 2), scale_dmg(5.0, 2), 10.0, 1.0, 1, RarityID::kEpic,
        {.constant_heal = scale_heal(1.5, 2), .icon_angle = -1}},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 1.0, 1, RarityID::kLegendary,
        {.constant_heal = scale_heal(1.5, 3), .icon_angle = -1}},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 1.0, 1, RarityID::kMythic,
        {.constant_heal = scale_heal(1.5, 4), .icon_angle = -1}},
    {"Yucca", "Heals the flower, but only while in the defensive position",
        scale_hp(10.0, 5), scale_dmg(5.0, 5), 10.0, 1.0, 1, RarityID::kUnique,
        {.constant_heal = scale_heal(1.5, 5), .icon_angle = -1}},
    // Bone — Δ from kBone (12/10 @ Legendary=4); Spawn.cc still keys armor
    // off PetalID::kBone only, so the new tiers are armorless. Authors can
    // bump that lookup later if they want graded armor.
    {"Bone", "Sturdy",
        scale_hp(12.0, -4), scale_dmg(10.0, -4), 12.0, 2.5, 1, RarityID::kCommon, {.icon_angle = 1}},
    {"Bone", "Sturdy",
        scale_hp(12.0, -3), scale_dmg(10.0, -3), 12.0, 2.5, 1, RarityID::kUnusual, {.icon_angle = 1}},
    {"Bone", "Sturdy",
        scale_hp(12.0, -2), scale_dmg(10.0, -2), 12.0, 2.5, 1, RarityID::kRare, {.icon_angle = 1}},
    {"Bone", "Sturdy",
        scale_hp(12.0, -1), scale_dmg(10.0, -1), 12.0, 2.5, 1, RarityID::kEpic, {.icon_angle = 1}},
    {"Bone", "Sturdy",
        scale_hp(12.0, 1), scale_dmg(10.0, 1), 12.0, 2.5, 1, RarityID::kMythic, {.icon_angle = 1}},
    {"Bone", "Sturdy",
        scale_hp(12.0, 2), scale_dmg(10.0, 2), 12.0, 2.5, 1, RarityID::kUnique, {.icon_angle = 1}},
    // Rice — Δ from kRice (1/4, reload=0.1 @ Epic=3)
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, -3), scale_dmg(4.0, -3), 13.0, 0.1, 1, RarityID::kCommon, {.icon_angle = 0.7}},
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, -2), scale_dmg(4.0, -2), 13.0, 0.1, 1, RarityID::kUnusual, {.icon_angle = 0.7}},
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, -1), scale_dmg(4.0, -1), 13.0, 0.1, 1, RarityID::kRare, {.icon_angle = 0.7}},
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, 1), scale_dmg(4.0, 1), 13.0, 0.1, 1, RarityID::kLegendary, {.icon_angle = 0.7}},
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, 2), scale_dmg(4.0, 2), 13.0, 0.1, 1, RarityID::kMythic, {.icon_angle = 0.7}},
    {"Rice", "Spawns instantly, but not very strong",
        scale_hp(1.0, 3), scale_dmg(4.0, 3), 13.0, 0.1, 1, RarityID::kUnique, {.icon_angle = 0.7}},
    // Corn — Δ from kCorn (500/2.5, reload=10 @ Epic=3)
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, -3), scale_dmg(2.5, -3), 16.0, 10.0, 1, RarityID::kCommon, {.icon_angle = 0.5}},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, -2), scale_dmg(2.5, -2), 16.0, 10.0, 1, RarityID::kUnusual, {.icon_angle = 0.5}},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, -1), scale_dmg(2.5, -1), 16.0, 10.0, 1, RarityID::kRare, {.icon_angle = 0.5}},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, 1), scale_dmg(2.5, 1), 16.0, 10.0, 1, RarityID::kLegendary, {.icon_angle = 0.5}},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, 2), scale_dmg(2.5, 2), 16.0, 10.0, 1, RarityID::kMythic, {.icon_angle = 0.5}},
    {"Corn", "Takes a long time to spawn, but has a lot of health",
        scale_hp(500.0, 3), scale_dmg(2.5, 3), 16.0, 10.0, 1, RarityID::kUnique, {.icon_angle = 0.5}},
    // Epic Peas — Δ from kPeas (5/8 count=4 @ Rare=2). Appended; see enum.
    {"Peas", "4 in 1 deal",
        scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 2.0, 4, RarityID::kEpic,
        {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, -3), scale_dmg(10.0, -3), 10.0, 1.0, 1, RarityID::kCommon, {
            .defend_only = 1
        }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 1.0, 1, RarityID::kUnusual, {
            .defend_only = 1
    }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 1.0, 1, RarityID::kRare, {
            .defend_only = 1
    }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, 0), scale_dmg(10.0, 0), 10.0, 1.0, 1, RarityID::kEpic, {
            .defend_only = 1
    }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 1.0, 1, RarityID::kLegendary, {
            .defend_only = 1
    }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 1.0, 1, RarityID::kMythic, {
            .defend_only = 1
    }},
    {"Root", "Slowly grants stacking armor that absorbs damage",
        scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1, RarityID::kUnique, {
            .defend_only = 1
    }},
};

struct MobData const MOB_DATA[MobID::kNumMobs] = {
    {
        "Baby Ant",
        "Weak and defenseless, but big dreams.",
        RarityID::kCommon, {10.0}, 10.0, {14.0}, 1, {
        PetalID::kLight, PetalID::kUnusualLight, PetalID::kRareLight, PetalID::kEpicLight, PetalID::kCommonLeaf, PetalID::kLeaf, PetalID::kRareLeaf, PetalID::kEpicLeaf, PetalID::kLegendaryLeaf, PetalID::kTwin, PetalID::kRice, PetalID::kTriplet
    }, {}},
    {
        "Worker Ant",
        "It's temperamental, probably from working all the time.",
        RarityID::kCommon, {25.0}, 10.0, {14.0}, 3, {
        PetalID::kLight, PetalID::kUnusualLight, PetalID::kRareLight, PetalID::kEpicLight, PetalID::kCommonLeaf, PetalID::kLeaf, PetalID::kRareLeaf, PetalID::kEpicLeaf, PetalID::kLegendaryLeaf, PetalID::kTwin, PetalID::kCorn, PetalID::kBone
    }, {}},
    {
        "Soldier Ant",
        "It's got wings and it's ready to use them.",
        RarityID::kUnusual, {40.0}, 10.0, {14.0}, 5, {
        PetalID::kTwin, PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kWing, PetalID::kFaster, PetalID::kTriplet
    }, {}},
    {
        "Bee",
        "It stings. Don't touch it.",
        RarityID::kCommon, {15.0}, 50.0, {20.0}, 4, {
        PetalID::kCommonStinger, PetalID::kStinger, PetalID::kRareStinger, PetalID::kEpicStinger, PetalID::kTringer, PetalID::kMythicTringer, PetalID::kPollen
    }, {}},
    {
        "Ladybug",
        "Cute and harmless.",
        RarityID::kCommon, {25.0}, 10.0, {30.0}, 3, {
        PetalID::kLight, PetalID::kUnusualLight, PetalID::kRareLight, PetalID::kEpicLight, PetalID::kCommonRose, PetalID::kRose, PetalID::kRareRose, PetalID::kLegendaryRose, PetalID::kTwin, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble
    }, {}},
    {
        "Beetle",
        "It's hungry and flowers are its favorite meal.",
        RarityID::kUnusual, {40.0}, 35.0, {35.0}, 10, {
        PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kSalt, PetalID::kWing, PetalID::kTriplet
    }, {}},
    {
        "Massive Ladybug",
        "Much larger, but still cute.",
        RarityID::kEpic, {1000.0}, 10.0, {90.0}, 400, {
        PetalID::kCommonRose, PetalID::kRose, PetalID::kRareRose, PetalID::kLegendaryRose, PetalID::kDahlia, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kAzalea, PetalID::kObserver
    }, {}},
    {
        "Massive Beetle",
        "Someone overfed this one, you might be next.",
        RarityID::kRare, {600.0}, 35.0, {75.0}, 50, {
        PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kWing, PetalID::kBlueIris, PetalID::kTriplet, PetalID::kBeetleEgg, PetalID::kThirdEye
    }, { .aggro_radius = 750 }},
    {
        "Ladybug",
        "Cute and harmless... if left unprovoked.",
        RarityID::kUnusual, {35.0}, 10.0, {30.0}, 5, {
        PetalID::kDahlia, PetalID::kWing, PetalID::kYinYang, PetalID::kAzalea
    }, {}},
    {
        "Hornet",
        "These aren't quite as nice as the little bees.",
        RarityID::kUnusual, {40.0}, 40.0, {40.0}, 12, {
        PetalID::kDandelion, PetalID::kMissile, PetalID::kWing, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kAntennae
    }, { .aggro_radius = 600 }},
    {
        "Cactus",
        "This one's prickly, don't touch it either.",
        RarityID::kCommon, {25.0, 50.0}, 30.0, {30.0, 60.0}, 2, {
        PetalID::kStinger, PetalID::kYucca, PetalID::kCactus, PetalID::kPoisonCactus, PetalID::kTricac
    }, { .stationary = 1 }},
    {
        "Rock",
        "A rock. It doesn't do much.",
        RarityID::kCommon, {5.0, 15.0}, 10.0, {10.0, 25.0}, 1, {
        PetalID::kHeavy, PetalID::kUnusualHeavy, PetalID::kRareHeavy, PetalID::kEpicHeavy, PetalID::kLegendaryHeavy, PetalID::kLight, PetalID::kUnusualLight, PetalID::kRareLight, PetalID::kEpicLight, PetalID::kCommonRock, PetalID::kUnusualRock, PetalID::kRock, PetalID::kEpicRock, PetalID::kLegendaryRock
    }, { .stationary = 1 }},
    {
        "Boulder",
        "A bigger rock. It also doesn't do much.",
        RarityID::kUnusual, {40.0, 60.0}, 10.0, {50.0, 75.0}, 1, {
        PetalID::kHeavy, PetalID::kUnusualHeavy, PetalID::kRareHeavy, PetalID::kEpicHeavy, PetalID::kLegendaryHeavy, PetalID::kCommonRock, PetalID::kUnusualRock, PetalID::kRock, PetalID::kEpicRock, PetalID::kLegendaryRock, PetalID::kHeaviest, PetalID::kMoon
    }, { .stationary = 1 }},
    {
        "Centipede",
        "It's just there doing its thing.",
        RarityID::kUnusual, {50.0}, 10.0, {35.0}, 2, {
        PetalID::kLight, PetalID::kUnusualLight, PetalID::kRareLight, PetalID::kEpicLight, PetalID::kTwin, PetalID::kCommonLeaf, PetalID::kLeaf, PetalID::kRareLeaf, PetalID::kEpicLeaf, PetalID::kLegendaryLeaf, PetalID::kPeas, PetalID::kTriplet
    }, { .segments = 10 }},
    {
        "Evil Centipede",
        "This one loves flowers.",
        RarityID::kRare, {50.0}, 10.0, {35.0}, 3, {
        PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kPoisonPeas, PetalID::kBlueIris
    }, { .segments = 10, .poison_damage = { 5.0, 2.0 } }},
    {
        "Desert Centipede",
        "It doesn't like it when you interrupt its run.",
        RarityID::kRare, {50.0}, 10.0, {35.0}, 4, {
        PetalID::kSand, PetalID::kFaster, PetalID::kSalt, PetalID::kStick
    }, { .segments = 6 }},
    {
        "Sandstorm",
        "Quite unpredictable.",
        RarityID::kUnusual, {30.0, 45.0}, 40.0, {32.0, 48.0}, 5, {
        PetalID::kSand, PetalID::kFaster, PetalID::kStick
    }, {}},
    {
        "Scorpion",
        "This one stings, now with poison.",
        RarityID::kUnusual, {35.0}, 15.0, {35.0}, 10, {
        PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kPincer, PetalID::kTriplet, PetalID::kLotus
    }, { .poison_damage = { 5.0, 1.0 } }},
    {
        "Spider",
        "Spooky.",
        RarityID::kUnusual, {35.0}, 10.0, {15.0}, 8, {
        PetalID::kStinger, PetalID::kWeb, PetalID::kFaster, PetalID::kTriweb
    }, { .poison_damage = { 5.0, 3.0 } }},
    {
        "Ant Hole",
        "Ants go in, and come out. Can't explain that.",
        RarityID::kRare, {500.0}, 10.0, {45.0}, 25, {
        PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kWing, PetalID::kAntEgg, PetalID::kTriplet
    }, { .stationary = 1 }},
    {
        "Queen Ant",
        "You must have done something really bad if she's chasing you.",
        RarityID::kRare, {350.0}, 10.0, {25.0}, 15, {
        PetalID::kTwin, PetalID::kCommonIris, PetalID::kIris, PetalID::kRareIris, PetalID::kLegendaryIris, PetalID::kWing, PetalID::kAntEgg, PetalID::kTringer
    }, { .aggro_radius = 750 }},
    {
        "Ladybug",
        "This one is shiny... I wonder what it could mean...",
        RarityID::kEpic, {25.0}, 10.0, {30.0}, 3, {
        PetalID::kDahlia, PetalID::kWing, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kYggdrasil
    }, {}},
    {
        "Square",
        "???",
        RarityID::kUnique, {20.0}, 10.0, {40.0}, 1, {
        PetalID::kSquare
    }, { .stationary = 1 }},
    {
        "Digger",
        "Friend or foe? You'll never know...",
        RarityID::kEpic, {250.0}, 25.0, {40.0}, 1, {
        PetalID::kCutter
    }, {}},
    {
        "Leafbug",
        "It looks like a leaf, but it's actually a bug.",
        RarityID::kCommon, {20.0}, 10.0, {30.0}, 2, {
        PetalID::kCommonLeaf, PetalID::kLeaf, PetalID::kRareLeaf, PetalID::kEpicLeaf, PetalID::kLegendaryLeaf, PetalID::kCommonRoot, PetalID::kUnusualRoot, PetalID::kRoot, PetalID::kEpicRoot, PetalID::kLegendaryRoot, PetalID::kMythicRoot, PetalID::kUniqueRoot
    }, {}},
    {
        "Bush",
        "It's a bush. It doesn't do much.",
        RarityID::kCommon, {20, 70.0}, 10.0, {20, 70.0}, 1, {
        PetalID::kCommonLeaf, PetalID::kLeaf, PetalID::kRareLeaf, PetalID::kEpicLeaf, PetalID::kLegendaryLeaf
    }, { .stationary = 1 }},
    {
        "Mantis",
        "It looks like it's praying, but it's actually waiting to strike.",
        RarityID::kCommon, {30.0}, 10.0, {30.0}, 3, {
        PetalID::kCommonPeas, PetalID::kUnusualPeas, PetalID::kPeas, PetalID::kEpicPeas, PetalID::kLegendaryPeas
    }, {}
    },
    {
        "Wasp",
        "It's aggressive and it stings. Watch out.",
        RarityID::kCommon, {40.0}, 40.0, {40.0}, 12, {
            PetalID::kCommonMissile, PetalID::kUnusualMissile, PetalID::kMissile, PetalID::kEpicMissile, PetalID::kLegendaryMissile, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kAntennae
    }, {}
    }
};

std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES = [](){
    std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> ret;
    double const RARITY_MULT[RarityID::kNumRarities] = {50000,15000,5000,1000,500,250,100};
    double MOB_SPAWN_RATES[MobID::kNumMobs] = {0};
    double PETAL_AGGREGATE_DROPS[PetalID::kNumPetals] = {0};
    for (struct ZoneDefinition const &zone : MAP) {
        double total = 0;
        for (SpawnChance const &s : zone.spawns) total += s.chance;
        for (SpawnChance const &s : zone.spawns) {
            double base_chance = (s.chance * zone.drop_multiplier / total);
            MOB_SPAWN_RATES[s.id] += base_chance;
            if (s.id == MobID::kAntHole) {
                MOB_SPAWN_RATES[MobID::kDigger] += DIGGER_SPAWN_CHANCE * base_chance;
                for (auto const &spawn_wave : ANTHOLE_SPAWNS)
                    for (MobID::T spawn : spawn_wave)
                        MOB_SPAWN_RATES[spawn] += base_chance;
            }
        }
    }

    for (MobID::T id = 0; id < MobID::kNumMobs; ++id)
        for (PetalID::T const drop_id : MOB_DATA[id].drops) PETAL_AGGREGATE_DROPS[drop_id]++;

    double const BASE_NUM = MOB_SPAWN_RATES[MobID::kSquare];
    if (BASE_NUM <= 0) assert(!"Square mob must spawn in at least one zone");

    for (MobID::T id = 0; id < MobID::kNumMobs; ++id) {
        for (PetalID::T const drop_id : MOB_DATA[id].drops) {
            float chance = fclamp((BASE_NUM * RARITY_MULT[PETAL_DATA[drop_id].rarity]) / (PETAL_AGGREGATE_DROPS[drop_id] * MOB_SPAWN_RATES[id] * MOB_DATA[id].attributes.segments), 0, 1);
            ret[id].push(chance);
        }
    }
    return ret;
}();

uint32_t score_to_pass_level(uint32_t level) {
    return (uint32_t)(pow(1.06, level - 1) * level) + 3;
}

uint32_t score_to_level(uint32_t score) {
    uint32_t level = 1;
    while (level < MAX_LEVEL) {
        uint32_t level_score = score_to_pass_level(level);
        if (score < level_score) break;
        score -= level_score;
        ++level;
    }
    return level;
}

uint32_t level_to_score(uint32_t level) {
    uint32_t score = 0;
    for (uint32_t i = 1; i < level; ++i)
        score += score_to_pass_level(i);
    return score;
}

uint32_t loadout_slots_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    uint32_t ret = 5 + level / LEVELS_PER_EXTRA_SLOT;
    if (ret > MAX_SLOT_COUNT) return MAX_SLOT_COUNT;
    return ret;
}

float hp_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return BASE_HEALTH + level;
}