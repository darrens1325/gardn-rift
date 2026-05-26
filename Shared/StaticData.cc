#include <Shared/StaticData.hh>

#include <Shared/Map.hh>

#include <cmath>
#include <cstring>

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

struct PetalData const PETAL_DATA[PetalType::kNumPetalTypes][RarityID::kNumRarities] = {
    [PetalType::kNone] = {
        [RarityID::kCommon] = {"None", "How can you see this?",
            0.0, 0.0, 0.0, 1.0, 0, {}},
    },
    [PetalType::kBasic] = {
        [RarityID::kCommon] = {"Basic", "A nice petal, not too strong but not too weak",
            10.0, 10.0, 10.0, 2.5, 1, {}},
        [RarityID::kUnusual] = {"Basic", "A nice petal, not too strong but not too weak",
            18.0, 16.0, 10.0, 2.5, 1, {}},
        [RarityID::kRare] = {"Basic", "A nice petal, not too strong but not too weak",
            30.0, 25.0, 10.0, 2.5, 1, {}},
        [RarityID::kEpic] = {"Basic", "A nice petal, not too strong but not too weak",
            50.0, 40.0, 10.0, 2.5, 1, {}},
        [RarityID::kLegendary] = {"Basic", "A nice petal, not too strong but not too weak",
            scale_hp(50.0, 1), scale_dmg(40.0, 1), 10.0, 2.5, 1, {}},
        [RarityID::kMythic] = {"Basic", "A nice petal, not too strong but not too weak",
            scale_hp(50.0, 2), scale_dmg(40.0, 2), 10.0, 2.5, 1, {}},
        [RarityID::kUltra] = {"Basic", "Something incredibly rare and useless",
            scale_hp(50.0, 3), scale_dmg(40.0, 3), 10.0, 2.5, 1, {}},
        [RarityID::kSuper] = {"Basic", "Something incredibly rare and useless",
            scale_hp(50.0, 4), scale_dmg(40.0, 4), 10.0, 2.5, 1, {}},
        [RarityID::kUnique] = {"Basic", "Something incredibly rare and useless",
            scale_hp(50.0, 5), scale_dmg(40.0, 5), 10.0, 2.5, 1, {}},
    },
    [PetalType::kLight] = {
        [RarityID::kCommon] = {"Fast", "Weaker than most petals, but reloads very quickly",
            5.0, 8.0, 7.0, 1.0, 1, {}},
        [RarityID::kUnusual] = {"Fast", "Weaker than most petals, but reloads very quickly",
            8.0, 14.0, 7.0, 1.0, 1, {}},
        [RarityID::kRare] = {"Fast", "Weaker than most petals, but reloads very quickly",
            12.0, 22.0, 7.0, 1.0, 1, {}},
        [RarityID::kEpic] = {"Fast", "Weaker than most petals, but reloads very quickly",
            20.0, 35.0, 7.0, 1.0, 1, {}},
        [RarityID::kLegendary] = {"Fast", "Weaker than most petals, but reloads very quickly",
            scale_hp(20.0, 1), scale_dmg(35.0, 1), 7.0, 1.0, 1, {}},
        [RarityID::kMythic] = {"Fast", "Weaker than most petals, but reloads very quickly",
            scale_hp(20.0, 2), scale_dmg(35.0, 2), 7.0, 1.0, 1, {}},
        [RarityID::kUltra] = {"Fast", "Weaker than most petals, but reloads very quickly",
            scale_hp(20.0, 3), scale_dmg(35.0, 3), 7.0, 1.0, 1, {}},
        [RarityID::kSuper] = {"Fast", "Weaker than most petals, but reloads very quickly",
            scale_hp(20.0, 3), scale_dmg(35.0, 3), 7.0, 1.0, 1, {}},
        [RarityID::kUnique] = {"Fast", "Weaker than most petals, but reloads very quickly",
            scale_hp(20.0, 3), scale_dmg(35.0, 3), 7.0, 1.0, 1, {}},
    },
    [PetalType::kHeavy] = {
        [RarityID::kCommon] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            20.0, 20.0, 12.0, 4.5, 1, {}},
        [RarityID::kUnusual] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            35.0, 30.0, 12.0, 4.5, 1, {}},
        [RarityID::kRare] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            60.0, 45.0, 12.0, 4.5, 1, {}},
        [RarityID::kEpic] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            100.0, 70.0, 12.0, 4.5, 1, {}},
        [RarityID::kLegendary] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            160.0, 100.0, 12.0, 4.5, 1, {}},
        [RarityID::kMythic] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            scale_hp(160.0, 1), scale_dmg(100.0, 1), 12.0, 4.5, 1, {}},
        [RarityID::kUltra] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            scale_hp(160.0, 2), scale_dmg(100.0, 2), 12.0, 4.5, 1, {}},
        [RarityID::kSuper] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            scale_hp(160.0, 2), scale_dmg(100.0, 2), 12.0, 4.5, 1, {}},
        [RarityID::kUnique] = {"Heavy", "Very resilient and deals more damage, but reloads very slowly",
            scale_hp(160.0, 2), scale_dmg(100.0, 2), 12.0, 4.5, 1, {}},
    },
    [PetalType::kStinger] = {
        [RarityID::kCommon] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 20.0, 7.0, 3.5, 1, {}},
        [RarityID::kUnusual] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 35.0, 7.0, 3.5, 1, {}},
        [RarityID::kRare] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 50.0, 7.0, 3.5, 1, {}},
        [RarityID::kEpic] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 75.0, 7.0, 3.5, 1, {}},
    },
    [PetalType::kTringer] = {
        [RarityID::kLegendary] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 35.0, 7.0, 4.5, 3, {
            .clump_radius = 10
        }},
        [RarityID::kMythic] = {"Stinger", "It really hurts, but it's really fragile",
            5.0, 50.0, 7.0, 4.5, 3, {
            .clump_radius = 10
        }},
        [RarityID::kUltra] = {"Stinger", "It really hurts, but it's really fragile",
            scale_hp(5.0, 1), scale_dmg(50.0, 1), 7.0, 4.5, 3, {.clump_radius = 10}},
        [RarityID::kSuper] = {"Stinger", "It really hurts, but it's really fragile",
            scale_hp(5.0, 1), scale_dmg(50.0, 1), 7.0, 4.5, 3, {.clump_radius = 10}},
        [RarityID::kUnique] = {"Stinger", "It really hurts, but it's really fragile",
            scale_hp(5.0, 1), scale_dmg(50.0, 1), 7.0, 4.5, 3, {.clump_radius = 10}},
    },
    [PetalType::kLeaf] = {
        [RarityID::kCommon] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            6.0, 6.0, 10.0, 1.0, 1, {
            .constant_heal = 0.5,
            .icon_angle = -1
        }},
        [RarityID::kUnusual] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            10.0, 8.0, 10.0, 1.0, 1, {
            .constant_heal = 1,
            .icon_angle = -1
        }},
        [RarityID::kRare] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            18.0, 12.0, 10.0, 1.0, 1, {
            .constant_heal = 1.5,
            .icon_angle = -1
        }},
        [RarityID::kEpic] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            30.0, 18.0, 10.0, 1.0, 1, {
            .constant_heal = 2.5,
            .icon_angle = -1
        }},
        [RarityID::kLegendary] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            50.0, 25.0, 10.0, 1.0, 1, {
            .constant_heal = 4.0,
            .icon_angle = -1
        }},
        [RarityID::kMythic] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            scale_hp(50.0, 1), scale_dmg(25.0, 1), 10.0, 1.0, 1, {.constant_heal = scale_heal(4.0, 1), .icon_angle = -1}},
        [RarityID::kUltra] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            scale_hp(50.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1, {.constant_heal = scale_heal(4.0, 2), .icon_angle = -1}},
        [RarityID::kSuper] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            scale_hp(50.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1, {.constant_heal = scale_heal(4.0, 2), .icon_angle = -1}},
        [RarityID::kUnique] = {"Leaf", "Gathers energy from the sun to passively heal your flower",
            scale_hp(50.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1, {.constant_heal = scale_heal(4.0, 2), .icon_angle = -1}},
    },
    [PetalType::kTwin] = {
        [RarityID::kCommon] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 1.0, 2, {}},
        [RarityID::kUnusual] = {"Twin", "Why stop at one? Why not TWO?!",
            5.0, 8.0, 7.0, 1.0, 2, {}},
        [RarityID::kRare] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 1.0, 2, {}},
        [RarityID::kEpic] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 1.0, 2, {}},
        [RarityID::kLegendary] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 2, {}},
        [RarityID::kMythic] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 1.0, 2, {}},
        [RarityID::kUltra] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 5), scale_dmg(8.0, 5), 7.0, 1.0, 2, {}},
        [RarityID::kSuper] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 5), scale_dmg(8.0, 5), 7.0, 1.0, 2, {}},
        [RarityID::kUnique] = {"Twin", "Why stop at one? Why not TWO?!",
            scale_hp(5.0, 5), scale_dmg(8.0, 5), 7.0, 1.0, 2, {}},
    },
    [PetalType::kRose] = {
        [RarityID::kCommon] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            3.0, 3.0, 10.0, 3.5, 1, {
            .secondary_reload = 1.0,
            .burst_heal = 5,
            .defend_only = 1
        }},
        [RarityID::kUnusual] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            5.0, 5.0, 10.0, 3.5, 1, {
            .secondary_reload = 1.0,
            .burst_heal = 10,
            .defend_only = 1
        }},
        [RarityID::kRare] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            7.0, 7.0, 10.0, 3.5, 1, {
            .secondary_reload = 1.0,
            .burst_heal = 15,
            .defend_only = 1
        }},
        [RarityID::kEpic] = {"Rose", "Extremely powerful rose, almost unheard of",
            5.0, 5.0, 10.0, 3.5, 1, {
            .secondary_reload = 1.0,
            .burst_heal = 22,
            .defend_only = 1
        }},
        [RarityID::kLegendary] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            10.0, 10.0, 10.0, 3.5, 1, {
            .secondary_reload = 1.0,
            .burst_heal = 35,
            .defend_only = 1
        }},
        [RarityID::kMythic] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 3.5, 1,
            {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 1), .defend_only = 1}},
        [RarityID::kUltra] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 3.5, 1,
            {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 2), .defend_only = 1}},
        [RarityID::kSuper] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 3.5, 1,
            {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 2), .defend_only = 1}},
        [RarityID::kUnique] = {"Rose", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 3.5, 1,
            {.secondary_reload = 1.0, .burst_heal = scale_heal(35.0, 2), .defend_only = 1}},
    },
    [PetalType::kAzalea] = {},
    [PetalType::kIris] = {
        [RarityID::kCommon] = {"Iris", "Very poisonous, but takes a while to do its work",
            3.0, 3.0, 7.0, 5.0, 1, {
            .poison_damage = { 5.0, 6.0 }
        }},
        [RarityID::kUnusual] = {"Iris", "Very poisonous, but takes a while to do its work",
             5.0, 5.0, 7.0, 5.0, 1, {
            .poison_damage = { 10.0, 6.0 }
        }},
        [RarityID::kRare] = {"Iris", "Very poisonous, but takes a while to do its work",
            7.0, 7.0, 7.0, 5.0, 1, {
            .poison_damage = { 20.0, 6.0 }
        }},
        [RarityID::kLegendary] = {"Iris", "Very poisonous, but takes a while to do its work",
            15.0, 15.0, 7.0, 5.0, 1, {
            .poison_damage = { 40.0, 5.0 }
        }},
        [RarityID::kMythic] = {"Iris", "Very poisonous, but takes a while to do its work",
            scale_hp(15.0, 1), scale_dmg(15.0, 1), 7.0, 5.0, 1,
            {.poison_damage = {scale_poison(40.0, 1), 5.0}}},
        [RarityID::kUltra] = {"Iris", "Very poisonous, but takes a while to do its work",
            scale_hp(15.0, 2), scale_dmg(15.0, 2), 7.0, 5.0, 1,
            {.poison_damage = {scale_poison(40.0, 2), 5.0}}},
        [RarityID::kSuper] = {"Iris", "Very poisonous, but takes a while to do its work",
            scale_hp(15.0, 2), scale_dmg(15.0, 2), 7.0, 5.0, 1,
            {.poison_damage = {scale_poison(40.0, 2), 5.0}}},
        [RarityID::kUnique] = {"Iris", "Very poisonous, but takes a while to do its work",
            scale_hp(15.0, 2), scale_dmg(15.0, 2), 7.0, 5.0, 1,
            {.poison_damage = {scale_poison(40.0, 2), 5.0}}},
    },
    [PetalType::kBlueIris] = {
        [RarityID::kEpic] = {"Iris", "Deals its effects quicker than traditional irises",
            10.0, 5.0, 7.0, 5.0, 1, {
            .poison_damage = { 15.0, 4.0 }
        }},
    },
    [PetalType::kMissile] = {
        [RarityID::kCommon] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, -2), scale_dmg(25.0, -2), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUnusual] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, -1), scale_dmg(25.0, -1), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kRare] = {"Missile", "You can actually shoot this one",
            5.0, 25.0, 10.0, 1.0, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot
        }},
        [RarityID::kEpic] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 1), scale_dmg(25.0, 1), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kLegendary] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 2), scale_dmg(25.0, 2), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kMythic] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 3), scale_dmg(25.0, 3), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUltra] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 4), scale_dmg(25.0, 4), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kSuper] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 4), scale_dmg(25.0, 4), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUnique] = {"Missile", "You can actually shoot this one",
            scale_hp(5.0, 4), scale_dmg(25.0, 4), 10.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1, .icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    },
    [PetalType::kDandelion] = {
        [RarityID::kCommon] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUnusual] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kRare] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            10.0, 10.0, 10.0, 1.0, 1, {
            .icon_angle = 1,
            .rotation_style = PetalAttributes::kFollowRot
        }},
        [RarityID::kEpic] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kLegendary] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kMythic] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUltra] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kSuper] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
        [RarityID::kUnique] = {"Dandelion", "Its interesting properties prevent healing effects on affected units",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 1.0, 1,
            {.icon_angle = 1, .rotation_style = PetalAttributes::kFollowRot}},
    },
    [PetalType::kBubble] = {
        [RarityID::kCommon] = {"Bubble", "You can right click to pop it and propel your flower",
            0.5, 0.0, 12.0, 3.0, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kUnusual] = {"Bubble", "You can right click to pop it and propel your flower",
            0.7, 0.0, 12.0, 2.5, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kRare] = {"Bubble", "You can right click to pop it and propel your flower",
            1.0, 0.0, 12.0, 2.0, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kEpic] = {"Bubble", "You can right click to pop it and propel your flower",
            1.5, 0.0, 12.0, 1.5, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kLegendary] = {"Bubble", "You can right click to pop it and propel your flower",
            2.0, 0.0, 12.0, 1.0, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kMythic] = {"Bubble", "You can right click to pop it and propel your flower",
            scale_hp(2.0, 1), 0.0, 12.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUltra] = {"Bubble", "You can right click to pop it and propel your flower",
            scale_hp(2.0, 2), 0.0, 12.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kSuper] = {"Bubble", "You can right click to pop it and propel your flower",
            scale_hp(2.0, 2), 0.0, 12.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUnique] = {"Bubble", "You can right click to pop it and propel your flower",
            scale_hp(2.0, 2), 0.0, 12.0, 1.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
    },
    [PetalType::kFaster] = {
        [RarityID::kCommon] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, -2), scale_dmg(7.0, -2), 7.0, 0.5, 1, {}},
        [RarityID::kUnusual] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, -1), scale_dmg(7.0, -1), 7.0, 0.5, 1, {}},
        [RarityID::kRare] = {"Faster", "It's so light it makes your other petals spin faster",
            5.0, 7.0, 7.0, 0.5, 1, {}},
        [RarityID::kEpic] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 1), scale_dmg(7.0, 1), 7.0, 0.5, 1, {}},
        [RarityID::kLegendary] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 2), scale_dmg(7.0, 2), 7.0, 0.5, 1, {}},
        [RarityID::kMythic] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 3), scale_dmg(7.0, 3), 7.0, 0.5, 1, {}},
        [RarityID::kUltra] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 4), scale_dmg(7.0, 4), 7.0, 0.5, 1, {}},
        [RarityID::kSuper] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 4), scale_dmg(7.0, 4), 7.0, 0.5, 1, {}},
        [RarityID::kUnique] = {"Faster", "It's so light it makes your other petals spin faster",
            scale_hp(5.0, 4), scale_dmg(7.0, 4), 7.0, 0.5, 1, {}},
    },
    [PetalType::kRock] = {
        [RarityID::kCommon] = {"Rock", "Even more durable, but slower to recharge",
            30.0, 5.0, 12.0, 7.5, 1, {}},
        [RarityID::kUnusual] = {"Rock", "Even more durable, but slower to recharge",
            60.0, 7.0, 12.0, 7.5, 1, {}},
        [RarityID::kRare] = {"Rock", "Even more durable, but slower to recharge",
            100.0, 10.0, 12.0, 7.5, 1, {}},
        [RarityID::kEpic] = {"Rock", "Even more durable, but slower to recharge",
            200.0, 15.0, 12.0, 7.5, 1, {}},
        [RarityID::kLegendary] = {"Rock", "Even more durable, but slower to recharge",
            350.0, 25.0, 12.0, 7.5, 1, {}},
        [RarityID::kMythic] = {"Rock", "Even more durable, but slower to recharge",
            scale_hp(350.0, 1), scale_dmg(25.0, 1), 12.0, 7.5, 1, {}},
        [RarityID::kUltra] = {"Rock", "Even more durable, but slower to recharge",
            scale_hp(350.0, 2), scale_dmg(25.0, 2), 12.0, 7.5, 1, {}},
        [RarityID::kSuper] = {"Rock", "Even more durable, but slower to recharge",
            scale_hp(350.0, 2), scale_dmg(25.0, 2), 12.0, 7.5, 1, {}},
        [RarityID::kUnique] = {"Rock", "Even more durable, but slower to recharge",
            scale_hp(350.0, 2), scale_dmg(25.0, 2), 12.0, 7.5, 1, {}},
    },
    [PetalType::kCactus] = {
        [RarityID::kCommon] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, -2), scale_dmg(5.0, -2), 15.0, 1.0, 1, {}},
        [RarityID::kUnusual] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, -1), scale_dmg(5.0, -1), 15.0, 1.0, 1, {}},
        [RarityID::kRare] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            15.0, 5.0, 15.0, 1.0, 1, {}},
        [RarityID::kMythic] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, 3), scale_dmg(5.0, 3), 15.0, 1.0, 1, {}},
        [RarityID::kUltra] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, 4), scale_dmg(5.0, 4), 15.0, 1.0, 1, {}},
        [RarityID::kSuper] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, 4), scale_dmg(5.0, 4), 15.0, 1.0, 1, {}},
        [RarityID::kUnique] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            scale_hp(15.0, 4), scale_dmg(5.0, 4), 15.0, 1.0, 1, {}},
    },
    [PetalType::kPoisonCactus] = {
        [RarityID::kEpic] = {"Cactus", "Turns your flower poisonous. Enemies will take poison damage on contact",
            15.0, 5.0, 10.0, 1.0, 1, {
            .poison_damage = { 1.0, 5.0 }
        }},
    },
    [PetalType::kTricac] = {
        [RarityID::kLegendary] = {"Cactus", "Not very strong, but somehow increases your maximum health",
            15.0, 5.0, 10.0, 1.0, 3, {
            .clump_radius = 10,
        }},
    },
    [PetalType::kWeb] = {
        [RarityID::kCommon] = {"Web", "It's really sticky",
            scale_hp(10.0, -2), scale_dmg(5.0, -2), 10.0, 3.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUnusual] = {"Web", "It's really sticky",
            scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 3.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kRare] = {"Web", "It's really sticky",
            10.0, 5.0, 10.0, 3.0, 1, {
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kEpic] = {"Web", "It's really sticky",
            scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 3.0, 1,
            {.secondary_reload = 0.5, .defend_only = 1}},
    },
    [PetalType::kTriweb] = {
        [RarityID::kLegendary] = {"Web", "It's really sticky",
            10.0, 5.0, 10.0, 3.0, 3, {
            .clump_radius = 10,
            .secondary_reload = 0.5,
            .defend_only = 1,
        }},
        [RarityID::kMythic] = {"Web", "It's really sticky",
            scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 3.0, 3,
            {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUltra] = {"Web", "It's really sticky",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 3.0, 3,
            {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kSuper] = {"Web", "It's really sticky",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 3.0, 3,
            {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUnique] = {"Web", "It's really sticky",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 3.0, 3,
            {.clump_radius = 10, .secondary_reload = 0.5, .defend_only = 1}},
    },
    [PetalType::kWing] = {
        [RarityID::kCommon] = {"Wing", "It comes and goes",
            scale_hp(15.0, -2), scale_dmg(15.0, -2), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUnusual] = {"Wing", "It comes and goes",
            scale_hp(15.0, -1), scale_dmg(15.0, -1), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kRare] = {"Wing", "It comes and goes",
            15.0, 15.0, 10.0, 2.5, 1, {
            .icon_angle = 1,
        }},
        [RarityID::kEpic] = {"Wing", "It comes and goes",
            scale_hp(15.0, 1), scale_dmg(15.0, 1), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kLegendary] = {"Wing", "It comes and goes",
            scale_hp(15.0, 2), scale_dmg(15.0, 2), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kMythic] = {"Wing", "It comes and goes",
            scale_hp(15.0, 3), scale_dmg(15.0, 3), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUltra] = {"Wing", "It comes and goes",
            scale_hp(15.0, 4), scale_dmg(15.0, 4), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kSuper] = {"Wing", "It comes and goes",
            scale_hp(15.0, 4), scale_dmg(15.0, 4), 10.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUnique] = {"Wing", "It comes and goes",
            scale_hp(15.0, 4), scale_dmg(15.0, 4), 10.0, 2.5, 1, {.icon_angle = 1}},
    },
    [PetalType::kPeas] = {
        [RarityID::kCommon] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, -2), scale_dmg(8.0, -2), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kUnusual] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kRare] = {"Peas", "4 in 1 deal",
            5.0, 8.0, 7.0, 2.0, 4, {
            .clump_radius = 8,
            .secondary_reload = 0.1,
            .defend_only = 1,
        }},
        [RarityID::kEpic] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kLegendary] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kMythic] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kUltra] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kSuper] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
        [RarityID::kUnique] = {"Peas", "4 in 1 deal",
            scale_hp(5.0, 4), scale_dmg(8.0, 4), 7.0, 2.0, 4,
            {.clump_radius = 8, .secondary_reload = 0.1, .defend_only = 1}},
    },
    [PetalType::kPoisonPeas] = {
        [RarityID::kEpic] = {"Peas", "4 in 1 deal, now with a secret ingredient: poison",
            5.0, 2.0, 7.0, 2.0, 4, {
            .clump_radius = 8,
            .secondary_reload = 0.1,
            .defend_only = 1,
            .poison_damage = { 20.0, 0.5 }
        }},
    },
    [PetalType::kSand] = {
        [RarityID::kCommon] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, -2), scale_dmg(3.0, -2), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kUnusual] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, -1), scale_dmg(3.0, -1), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kRare] = {"Sand", "It's coarse, rough, and gets everywhere",
            10.0, 3.0, 7.0, 1.5, 4, {
            .clump_radius = 10,
        }},
        [RarityID::kEpic] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 1), scale_dmg(3.0, 1), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kLegendary] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 2), scale_dmg(3.0, 2), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kMythic] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 3), scale_dmg(3.0, 3), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kUltra] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 4), scale_dmg(3.0, 4), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kSuper] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 4), scale_dmg(3.0, 4), 7.0, 1.5, 4, {.clump_radius = 10}},
        [RarityID::kUnique] = {"Sand", "It's coarse, rough, and gets everywhere",
            scale_hp(10.0, 4), scale_dmg(3.0, 4), 7.0, 1.5, 4, {.clump_radius = 10}},
    },
    [PetalType::kPincer] = {
        [RarityID::kCommon] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, -2), scale_dmg(5.0, -2), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, -2), 1.0}}},
        [RarityID::kUnusual] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, -1), 1.0}}},
        [RarityID::kRare] = {"Pincer", "Stuns and poisons targets for a short duration",
            10.0, 5.0, 10.0, 2.5, 1, {
            .icon_angle = 0.7,
            .poison_damage = { 5.0, 1.0 }
        }},
        [RarityID::kEpic] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 1), 1.0}}},
        [RarityID::kLegendary] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 2), scale_dmg(5.0, 2), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 2), 1.0}}},
        [RarityID::kMythic] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 3), 1.0}}},
        [RarityID::kUltra] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 4), 1.0}}},
        [RarityID::kSuper] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 4), 1.0}}},
        [RarityID::kUnique] = {"Pincer", "Stuns and poisons targets for a short duration",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 2.5, 1,
            {.icon_angle = 0.7, .poison_damage = {scale_poison(5.0, 4), 1.0}}},
    },
    [PetalType::kDahlia] = {
        [RarityID::kCommon] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, -2), scale_dmg(5.0, -2), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, -2), .defend_only = 1}},
        [RarityID::kUnusual] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, -1), scale_dmg(5.0, -1), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, -1), .defend_only = 1}},
        [RarityID::kRare] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            5.0, 5.0, 7.0, 3.5, 3, {
            .clump_radius = 10,
            .secondary_reload = 1.0,
            .burst_heal = 3.5,
            .defend_only = 1
        }},
        [RarityID::kEpic] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 1), scale_dmg(5.0, 1), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 1), .defend_only = 1}},
        [RarityID::kLegendary] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 2), scale_dmg(5.0, 2), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 2), .defend_only = 1}},
        [RarityID::kMythic] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 3), scale_dmg(5.0, 3), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 3), .defend_only = 1}},
        [RarityID::kUltra] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 4), scale_dmg(5.0, 4), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 4), .defend_only = 1}},
        [RarityID::kSuper] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 4), scale_dmg(5.0, 4), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 4), .defend_only = 1}},
        [RarityID::kUnique] = {"Dahlia", "Its healing properties are amazing. Not so good at combat though",
            scale_hp(5.0, 4), scale_dmg(5.0, 4), 7.0, 3.5, 3,
            {.clump_radius = 10, .secondary_reload = 1.0, .burst_heal = scale_heal(3.5, 4), .defend_only = 1}},
    },
    [PetalType::kTriplet] = {
        [RarityID::kCommon] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, -3), scale_dmg(8.0, -3), 7.0, 1.0, 3, {}},
        [RarityID::kUnusual] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, -2), scale_dmg(8.0, -2), 7.0, 1.0, 3, {}},
        [RarityID::kRare] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, -1), scale_dmg(8.0, -1), 7.0, 1.0, 3, {}},
        [RarityID::kEpic] = {"Triplet", "How about THREE?!",
            5.0, 8.0, 7.0, 1.0, 3, {}},
        [RarityID::kLegendary] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, 1), scale_dmg(8.0, 1), 7.0, 1.0, 3, {}},
        [RarityID::kMythic] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, 2), scale_dmg(8.0, 2), 7.0, 1.0, 3, {}},
        [RarityID::kUltra] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 3, {}},
        [RarityID::kSuper] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 3, {}},
        [RarityID::kUnique] = {"Triplet", "How about THREE?!",
            scale_hp(5.0, 3), scale_dmg(8.0, 3), 7.0, 1.0, 3, {}},
    },
    [PetalType::kAntEgg] = {
        [RarityID::kEpic] = {"Egg", "Something interesting might pop out of this",
            50.0, 1.0, 12.5, 1.0, 2, {
            .secondary_reload = 3.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kSoldierAnt
        }},
    },
    [PetalType::kBeetleEgg] = {
        [RarityID::kEpic] = {"Egg", "Something interesting might pop out of this",
            50.0, 1.0, 15.0, 1.0, 1, {
            .secondary_reload = 3.5,
            .defend_only = 1,
            .rotation_style = PetalAttributes::kNoRot,
            .spawns = MobID::kBeetle
        }},
    },
    [PetalType::kPollen] = {
        [RarityID::kCommon] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, -3), scale_dmg(8.0, -3), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUnusual] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, -2), scale_dmg(8.0, -2), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kRare] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, -1), scale_dmg(8.0, -1), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kEpic] = {"Pollen", "Asthmatics beware",
            7.0, 8.0, 7.0, 1.5, 3, {
            .secondary_reload = 0.5,
            .defend_only = 1
        }},
        [RarityID::kLegendary] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, 1), scale_dmg(8.0, 1), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kMythic] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, 2), scale_dmg(8.0, 2), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUltra] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, 3), scale_dmg(8.0, 3), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kSuper] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, 3), scale_dmg(8.0, 3), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
        [RarityID::kUnique] = {"Pollen", "Asthmatics beware",
            scale_hp(7.0, 3), scale_dmg(8.0, 3), 7.0, 1.5, 3,
            {.secondary_reload = 0.5, .defend_only = 1}},
    },
    [PetalType::kStick] = {
        [RarityID::kLegendary] = {"Stick", "Harnesses the power of the wind",
            10.0, 1.0, 15.0, 3.0, 1, {
            .secondary_reload = 4.0,
            .defend_only = 1,
            .icon_angle = 1,
            .spawns = MobID::kSandstorm,
            .spawn_count = 2
        }},
    },
    [PetalType::kAntennae] = {
        [RarityID::kLegendary] = {"Antennae", "Allows your flower to sense foes from farther away",
            0.0, 0.0, 12.5, 0.0, 0, {}},
    },
    [PetalType::kHeaviest] = {
        [RarityID::kEpic] = {"Heaviest", "This thing is so heavy that nothing gets in the way",
            200.0, 10.0, 12.0, 15.0, 1, {
            .mass = 10,
            .rotation_style = PetalAttributes::kNoRot
        }},
    },
    [PetalType::kThirdEye] = {
        [RarityID::kMythic] = {"Third Eye", "Allows your flower to extend petals further out",
            0.0, 0.0, 20.0, 0.0, 0, {}},
    },
    [PetalType::kObserver] = {
        [RarityID::kMythic] = {"Observer", "The one who sees all",
            0.0, 0.0, 12.5, 0.0, 0, {}},
    },
    [PetalType::kSalt] = {
        [RarityID::kCommon] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 2.5, 1, {}},
        [RarityID::kUnusual] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 2.5, 1, {}},
        [RarityID::kRare] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            10.0, 10.0, 10.0, 2.5, 1, {}},
        [RarityID::kEpic] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 2.5, 1, {}},
        [RarityID::kLegendary] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 2.5, 1, {}},
        [RarityID::kMythic] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 2.5, 1, {}},
        [RarityID::kUltra] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 2.5, 1, {}},
        [RarityID::kSuper] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 2.5, 1, {}},
        [RarityID::kUnique] = {"Salt", "Reflects some damage dealt to the flower. Does not stack with itself",
            scale_hp(10.0, 4), scale_dmg(10.0, 4), 10.0, 2.5, 1, {}},
    },
    [PetalType::kSquare] = {
        [RarityID::kUnique] = {"Square", "This shape... it looks familiar...",
            10.0, 10.0, 15.0, 2.5, 1, {
            .icon_angle = M_PI / 4 + 1
        }},
    },
    [PetalType::kMoon] = {
        [RarityID::kMythic] = {"Moon", "Where did this come from?",
            1000.0, 1.0, 50.0, 10.0, 1, {
            .secondary_reload = 0.5,
            .mass = 200
        }},
    },
    [PetalType::kLotus] = {
        [RarityID::kEpic] = {"Lotus", "Absorbs some poison damage taken by the flower",
            5.0, 5.0, 12.0, 2.0, 1, {
            .icon_angle = 0.1
        }},
    },
    [PetalType::kCutter] = {
        [RarityID::kEpic] = {"Cutter", "Increases the flower's body damage",
            0.0, 0.0, 40.0, 0.0, 0, {}},
    },
    [PetalType::kYinYang] = {
        [RarityID::kEpic] = {"Yin Yang", "Alters the flower's petal rotation in interesting ways",
            15.0, 15.0, 10.0, 2.5, 1, {}},
    },
    [PetalType::kYggdrasil] = {
        [RarityID::kCommon] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -6), scale_dmg(1.0, -6), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kUnusual] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -5), scale_dmg(1.0, -5), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kRare] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -4), scale_dmg(1.0, -4), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kEpic] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -3), scale_dmg(1.0, -3), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kLegendary] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -2), scale_dmg(1.0, -2), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kMythic] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            scale_hp(1.0, -1), scale_dmg(1.0, -1), 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kUltra] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            1.0, 1.0, 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kSuper] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            1.0, 1.0, 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
        [RarityID::kUnique] = {"Yggdrasil", "Unfortunately, its powers are useless here",
            1.0, 1.0, 12.0, 10.0, 1, {
            .defend_only = 1,
            .icon_angle = M_PI
        }},
    },
    [PetalType::kRice] = {
        [RarityID::kCommon] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, -3), scale_dmg(4.0, -3), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kUnusual] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, -2), scale_dmg(4.0, -2), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kRare] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, -1), scale_dmg(4.0, -1), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kEpic] = {"Rice", "Spawns instantly, but not very strong",
            1.0, 4.0, 13.0, 0.1, 1, {
            .icon_angle = 0.7
        }},
        [RarityID::kLegendary] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, 1), scale_dmg(4.0, 1), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kMythic] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, 2), scale_dmg(4.0, 2), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kUltra] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, 3), scale_dmg(4.0, 3), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kSuper] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, 3), scale_dmg(4.0, 3), 13.0, 0.1, 1, {.icon_angle = 0.7}},
        [RarityID::kUnique] = {"Rice", "Spawns instantly, but not very strong",
            scale_hp(1.0, 3), scale_dmg(4.0, 3), 13.0, 0.1, 1, {.icon_angle = 0.7}},
    },
    [PetalType::kBone] = {
        [RarityID::kCommon] = {"Bone", "Sturdy",
            scale_hp(12.0, -4), scale_dmg(10.0, -4), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUnusual] = {"Bone", "Sturdy",
            scale_hp(12.0, -3), scale_dmg(10.0, -3), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kRare] = {"Bone", "Sturdy",
            scale_hp(12.0, -2), scale_dmg(10.0, -2), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kEpic] = {"Bone", "Sturdy",
            scale_hp(12.0, -1), scale_dmg(10.0, -1), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kLegendary] = {"Bone", "Sturdy",
            12.0, 10.0, 12.0, 2.5, 1, {
            .icon_angle = 1
        }},
        [RarityID::kMythic] = {"Bone", "Sturdy",
            scale_hp(12.0, 1), scale_dmg(10.0, 1), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUltra] = {"Bone", "Sturdy",
            scale_hp(12.0, 2), scale_dmg(10.0, 2), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kSuper] = {"Bone", "Sturdy",
            scale_hp(12.0, 2), scale_dmg(10.0, 2), 12.0, 2.5, 1, {.icon_angle = 1}},
        [RarityID::kUnique] = {"Bone", "Sturdy",
            scale_hp(12.0, 2), scale_dmg(10.0, 2), 12.0, 2.5, 1, {.icon_angle = 1}},
    },
    [PetalType::kYucca] = {
        [RarityID::kCommon] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, -1), scale_dmg(5.0, -1), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, -1), .icon_angle = -1}},
        [RarityID::kUnusual] = {"Yucca", "Heals the flower, but only while in the defensive position",
            10.0, 5.0, 10.0, 1.0, 1, {
            .constant_heal = 1.5,
            .icon_angle = -1
        }},
        [RarityID::kRare] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 1), scale_dmg(5.0, 1), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 1), .icon_angle = -1}},
        [RarityID::kEpic] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 2), scale_dmg(5.0, 2), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 2), .icon_angle = -1}},
        [RarityID::kLegendary] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 3), scale_dmg(5.0, 3), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 3), .icon_angle = -1}},
        [RarityID::kMythic] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 4), scale_dmg(5.0, 4), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 4), .icon_angle = -1}},
        [RarityID::kUltra] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 5), scale_dmg(5.0, 5), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 5), .icon_angle = -1}},
        [RarityID::kSuper] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 5), scale_dmg(5.0, 5), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 5), .icon_angle = -1}},
        [RarityID::kUnique] = {"Yucca", "Heals the flower, but only while in the defensive position",
            scale_hp(10.0, 5), scale_dmg(5.0, 5), 10.0, 1.0, 1,
            {.constant_heal = scale_heal(1.5, 5), .icon_angle = -1}},
    },
    [PetalType::kCorn] = {
        [RarityID::kCommon] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, -3), scale_dmg(2.5, -3), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kUnusual] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, -2), scale_dmg(2.5, -2), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kRare] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, -1), scale_dmg(2.5, -1), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kEpic] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            500.0, 2.5, 16.0, 10.0, 1, {
            .icon_angle = 0.5
        }},
        [RarityID::kLegendary] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, 1), scale_dmg(2.5, 1), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kMythic] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, 2), scale_dmg(2.5, 2), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kUltra] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, 3), scale_dmg(2.5, 3), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kSuper] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, 3), scale_dmg(2.5, 3), 16.0, 10.0, 1, {.icon_angle = 0.5}},
        [RarityID::kUnique] = {"Corn", "Takes a long time to spawn, but has a lot of health",
            scale_hp(500.0, 3), scale_dmg(2.5, 3), 16.0, 10.0, 1, {.icon_angle = 0.5}},
    },
    [PetalType::kRoot] = {
        [RarityID::kCommon] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, -3), scale_dmg(10.0, -3), 10.0, 1.0, 1, {
                .defend_only = 1
            }},
        [RarityID::kUnusual] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, -2), scale_dmg(10.0, -2), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kRare] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, -1), scale_dmg(10.0, -1), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kEpic] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 0), scale_dmg(10.0, 0), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kLegendary] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 1), scale_dmg(10.0, 1), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kMythic] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 2), scale_dmg(10.0, 2), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kUltra] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kSuper] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
        [RarityID::kUnique] = {"Root", "Slowly grants stacking armor that absorbs damage",
            scale_hp(10.0, 3), scale_dmg(10.0, 3), 10.0, 1.0, 1, {
                .defend_only = 1
        }},
    },
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
        PetalID::kCommonRose, PetalID::kRose, PetalID::kRareRose, PetalID::kLegendaryRose, PetalID::kDahlia, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kObserver
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
        PetalID::kDahlia, PetalID::kWing, PetalID::kYinYang
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
        PetalID::kDahlia, PetalID::kWing, PetalID::kCommonBubble, PetalID::kUnusualBubble, PetalID::kBubble, PetalID::kEpicBubble, PetalID::kLegendaryBubble, PetalID::kCommonYggdrasil, PetalID::kUnusualYggdrasil, PetalID::kRareYggdrasil, PetalID::kEpicYggdrasil, PetalID::kLegendaryYggdrasil, PetalID::kMythicYggdrasil, PetalID::kYggdrasil
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
    double const RARITY_MULT[RarityID::kNumRarities] = {50000,15000,5000,1000,500,250,175,130,100,50};
    double MOB_SPAWN_RATES[MobID::kNumMobs] = {0};
    double PETAL_AGGREGATE_DROPS[PetalType::kNumPetalTypes][RarityID::kNumRarities] = {{0}};
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
        for (PetalID::T const drop_id : MOB_DATA[id].drops) PETAL_AGGREGATE_DROPS[drop_id.type][drop_id.rarity]++;

    double const BASE_NUM = MOB_SPAWN_RATES[MobID::kSquare];
    if (BASE_NUM <= 0) assert(!"Square mob must spawn in at least one zone");

    for (MobID::T id = 0; id < MobID::kNumMobs; ++id) {
        for (PetalID::T const drop_id : MOB_DATA[id].drops) {
            float chance = fclamp((BASE_NUM * RARITY_MULT[drop_id.rarity]) / (PETAL_AGGREGATE_DROPS[drop_id.type][drop_id.rarity] * MOB_SPAWN_RATES[id] * MOB_DATA[id].attributes.segments), 0, 1);
            ret[id].push(chance);
        }
    }
    return ret;
}();

// Port of Scripts/drop_constant.py loot_table_gen(n). Returns a
// mob_rarity × drop_rarity table; cell [m][d] is the probability that a
// mob of rarity m drops a petal of rarity d for a single drop entry whose
// per-(mob, drop) exponent is `n`. Higher n → steeper rarity falloff.
//
// Two variants: the original capped version is used on NORMAL (non-BR)
// maps so the gardn loot table stays in sync with Scripts/drop_constant.py;
// the cap-free version is used for BR maps (and for generating
// MOB_DROP_CHANCES_BR via .ocr_work/build_inl.py) so high-tier drops can
// reach low-rarity mobs as florr.io's mob_gallery shows.
static constexpr double DROPS_BOUNDS[RarityID::kNumRarities + 1] = {
    0.0,
    0.8589559816476924,
    0.9963889387113232,
    0.9998247626379139,
    0.9999965538342435,
    0.9999999896581699,
    0.9999999999656417,
    0.99999999999988851,
    0.999999999999996150,
    0.9999999999999998750,
    1.0,
};
static constexpr double MOBS_DIVISOR[RarityID::kNumRarities] = {
    60000, 15000, 1500, 100, 5, 0.1, 0.0171, 0.00293, 0.0005, 0.0001,
};

static std::array<std::array<double, RarityID::kNumRarities>, RarityID::kNumRarities>
_loot_table_gen(double n) {
    // Capped variant: matches Scripts/drop_constant.py exactly. For
    // mob_rarity m, only drop_rarities 0..max(1,m) get non-zero mass;
    // the highest of those gets end=1 so the per-mob row sums to 1.
    std::array<std::array<double, RarityID::kNumRarities>, RarityID::kNumRarities> table{};
    for (int mob = 0; mob < (int)RarityID::kNumRarities; ++mob) {
        int cap = mob == 0 ? 1 : mob;
        if (cap >= (int)RarityID::kNumRarities) cap = (int)RarityID::kNumRarities - 1;
        double exponent = 300000.0 / MOBS_DIVISOR[mob];
        for (int drop = 0; drop <= cap; ++drop) {
            double start = DROPS_BOUNDS[drop];
            double end = (drop == cap) ? 1.0 : DROPS_BOUNDS[drop + 1];
            double base1 = n * start + (1.0 - n);
            double base2 = n * end + (1.0 - n);
            table[mob][drop] = std::pow(base2, exponent) - std::pow(base1, exponent);
        }
    }
    return table;
}

static std::array<std::array<double, RarityID::kNumRarities>, RarityID::kNumRarities>
_loot_table_gen_br(double n) {
    // Cap-free variant used on BR maps. Iterates drop_rarity
    // 0..kNumRarities-1 always; only the highest tier gets end=1. This
    // matches the wider distribution florr.io's gallery shows (e.g.,
    // Bee at Common can still drop Rare/Epic/Legendary Stinger with
    // tiny but non-zero chance). MOBS_DIVISOR_BR and DROPS_BOUNDS_BR
    // were tuned by .ocr_work/tune_constants.py against the 789
    // observations transcribed from mob_gallery.mov; the peaks land
    // closer to each mob's own rarity than the original constants do.
    static constexpr double DROPS_BOUNDS_BR[RarityID::kNumRarities + 1] = {
        0.0,
        0.8280141620166028,
        0.9994213801374269,
        0.9999959838524433,
        0.9999999965213968,
        0.9999999983920225,
        0.9999999983920242,
        0.99999999839202495,
        0.999999998392025,
        0.9999999983920257,
        1.0,
    };
    static constexpr double MOBS_DIVISOR_BR[RarityID::kNumRarities] = {
        24431.83968444404, 37424.40407882193, 530.0317372834454,
        144.8476629810749, 1.0432580614860787,
        0.000977177224351313, 0.000814,
        0.000727, 0.0006446370750282694, 0.0002,
    };
    std::array<std::array<double, RarityID::kNumRarities>, RarityID::kNumRarities> table{};
    int const last = (int)RarityID::kNumRarities - 1;
    for (int mob = 0; mob < (int)RarityID::kNumRarities; ++mob) {
        double exponent = 300000.0 / MOBS_DIVISOR_BR[mob];
        for (int drop = 0; drop <= last; ++drop) {
            double start = DROPS_BOUNDS_BR[drop];
            double end = (drop == last) ? 1.0 : DROPS_BOUNDS_BR[drop + 1];
            double base1 = n * start + (1.0 - n);
            double base2 = n * end + (1.0 - n);
            table[mob][drop] = std::pow(base2, exponent) - std::pow(base1, exponent);
        }
    }
    return table;
}

// Per-(mob, drop_index) exponent fed to _loot_table_gen. Indexes are
// into MOB_DATA[id].drops in authored order. Default 0.5; add explicit
// overrides below for drops that should fall off faster (n→1) or slower
// (n→0) than the baseline. Use the `n_for` helper to address overrides
// by PetalID instead of by drop-array index — the index would silently
// shift if a mob's drop list is reordered.
static std::array<std::array<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_N = [](){
    std::array<std::array<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> arr;
    for (auto &row : arr) row.fill(0.5f);
    auto n_for = [&](MobID::T mob, PetalID::T petal, float n) {
        for (uint32_t i = 0; i < MOB_DATA[mob].drops.size(); ++i) {
            if (MOB_DATA[mob].drops[i] == petal) { arr[mob][i] = n; return; }
        }
        // If the petal isn't authored on this mob, fail loudly in debug
        // builds — the override would silently do nothing otherwise.
        DEBUG_ONLY(assert(!"MOB_DROP_N override: petal not in mob's drops");)
    };
    (void)n_for;
    // Overrides. Petal/mob names referenced in design (Air via Bubble,
    // Corruption via Gambler) are not yet in this repo; add upstream first.
    // n_for(MobID::kBubble,  PetalID::kAir,        1.0f);
    // n_for(MobID::kGambler, PetalID::kCorruption, 1.0f);
    // n_for(MobID::kBabyAnt, PetalID::kCommonRice, 0.5f); // matches default
    return arr;
}();

#include "MobDropChancesBR.inl"

// 3D table: chances[mob_id][view_rarity][drop_idx]. Each row is the
// distribution florr.io's gallery shows for that mob at that view
// rarity. Cells with an explicit value in MobDropChancesBR.inl
// (snapshot entries are tagged with their `view_rarity`) win; missing
// cells fall back to _loot_table_gen_br evaluated at the view's rarity
// so post-snapshot petals still drop at all tiers.
std::array<
    std::array<StaticArray<float, MAX_DROPS_PER_MOB>, RarityID::kNumRarities>,
    MobID::kNumMobs> const MOB_DROP_CHANCES_BR = [](){
    std::array<
        std::array<StaticArray<float, MAX_DROPS_PER_MOB>, RarityID::kNumRarities>,
        MobID::kNumMobs> ret;
    for (MobID::T id = 0; id < MobID::kNumMobs; ++id) {
        char const *mname = MOB_DATA[id].name;
        for (uint8_t view = 0; view < RarityID::kNumRarities; ++view) {
            // Precompute the loot_table_gen_br row for this view (used
            // when a (mob, view, drop) entry is missing from the snapshot).
            // n is per-drop, so we recompute inside the inner loop.
            for (uint32_t i = 0; i < MOB_DATA[id].drops.size(); ++i) {
                PetalID::T did = MOB_DATA[id].drops[i];
                char const *pname = PETAL_DATA[did.type][did.rarity].name;
                float chance = -1.0f; // sentinel for "no snapshot entry"
                if (mname != nullptr && pname != nullptr) {
                    for (BRSnapshotEntry const &e : BR_DROP_SNAPSHOT) {
                        if (e.view_rarity == view
                            && e.petal_rarity == did.rarity
                            && std::strcmp(e.mob_name, mname) == 0
                            && std::strcmp(e.petal_name, pname) == 0) {
                            chance = e.chance;
                            break;
                        }
                    }
                }
                if (chance < 0.0f) {
                    uint8_t dr = did.rarity;
                    if (dr >= RarityID::kNumRarities) dr = RarityID::kNumRarities - 1;
                    double c = _loot_table_gen_br((double)MOB_DROP_N[id][i])[view][dr];
                    if (c < 0.0) c = 0.0;
                    if (c > 1.0) c = 1.0;
                    chance = (float)c;
                }
                ret[id][view].push(chance);
            }
        }
    }
    return ret;
}();

// Parallel to MOB_DROP_CHANCES, but computed from the ported
// Scripts/drop_constant.py loot_table_gen() with per-(mob, drop) n from
// MOB_DROP_N. Used on non-BR maps (anything whose map_path doesn't sit
// under Map/br/). BR maps use MOB_DROP_CHANCES_BR (above), which is the
// snapshot of the table the in-game Mob Gallery (mob_gallery.mov) records.
std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> const MOB_DROP_CHANCES_NORMAL = [](){
    std::array<StaticArray<float, MAX_DROPS_PER_MOB>, MobID::kNumMobs> ret;
    for (MobID::T id = 0; id < MobID::kNumMobs; ++id) {
        uint8_t mob_rarity = MOB_DATA[id].rarity;
        if (mob_rarity >= RarityID::kNumRarities) mob_rarity = RarityID::kNumRarities - 1;
        for (uint32_t i = 0; i < MOB_DATA[id].drops.size(); ++i) {
            PetalID::T drop = MOB_DATA[id].drops[i];
            uint8_t dr = drop.rarity;
            if (dr >= RarityID::kNumRarities) dr = RarityID::kNumRarities - 1;
            auto table = _loot_table_gen((double)MOB_DROP_N[id][i]);
            double chance = table[mob_rarity][dr];
            if (chance < 0.0) chance = 0.0;
            if (chance > 1.0) chance = 1.0;
            ret[id].push((float)chance);
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

// One extra slot per rarity step above Common: Common→5, Unusual→6,
// Rare→7, Epic+→capped at MAX_SLOT_COUNT.
uint32_t loadout_slots_for_max_rarity(uint8_t max_rarity) {
    uint32_t ret = 5 + max_rarity;
    if (ret > MAX_SLOT_COUNT) return MAX_SLOT_COUNT;
    return ret;
}

float hp_at_level(uint32_t level) {
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return BASE_HEALTH + level;
}