#pragma once

#include <Shared/Helpers.hh>

#include <cstdint>

inline uint32_t const ARENA_WIDTH = 20000;
inline uint32_t const ARENA_HEIGHT = 20000;

inline uint32_t const MAX_SLOT_COUNT = 8;
inline uint32_t const LEVELS_PER_EXTRA_SLOT = 15;
inline uint32_t const LEADERBOARD_SIZE = 10;
inline uint32_t const MAX_PETALS_IN_CLUMP = 4;
inline uint32_t const MAX_DIFFICULTY = 3;
inline uint32_t const MAX_DROPS_PER_MOB = 16;

// Game-time tick count of one wave / round. At 72000 game-ticks (=
// SIM_RATE × 3600 = 1 game-hour), the server resets: every flower is
// killed, every inventory wiped, the wave-tick counter rewinds to 0,
// and a kRoundEnd packet is broadcast naming the player with the highest
// score at the moment of reset. Mob rarity ramps linearly with wave_tick:
//   wave_rarity = clamp(wave_tick * kNumRarities / WAVE_TICKS_PER_ROUND, 0, 6)
// Common at the start of the round, Unique just before the reset.
inline uint32_t const WAVE_TICKS_PER_ROUND = 72000;

namespace DamageType {
    enum : uint8_t {
        kContact,
        kPoison,
        kReflect
    };
}

namespace PetalID {
    typedef uint8_t T;
    enum : T {
        kNone,
        kBasic,
        kUnusualBasic,
        kRareBasic,
        kEpicBasic,
        kLight,
        kUnusualLight,
        kRareLight,
        kEpicLight,
        kHeavy,
        kUnusualHeavy,
        kRareHeavy,
        kEpicHeavy,
        kLegendaryHeavy,
        kCommonStinger,
        kStinger,
        kRareStinger,
        kEpicStinger,
        kCommonLeaf,
        kLeaf,
        kRareLeaf,
        kEpicLeaf,
        kLegendaryLeaf,
        kTwin,
        kCommonRose,
        kRose,
        kRareRose,
        kLegendaryRose,
        kCommonIris,
        kIris,
        kRareIris,
        kLegendaryIris,
        kMissile,
        kDandelion,
        kCommonBubble,
        kUnusualBubble,
        kBubble,
        kEpicBubble,
        kLegendaryBubble,
        kFaster,
        kCommonRock,
        kUnusualRock,
        kRock,
        kEpicRock,
        kLegendaryRock,
        kCactus,
        kWeb,
        kWing,
        kPeas,
        kSand,
        kPincer,
        kDahlia,
        kTriplet,
        kAntEgg,
        kBlueIris,
        kPollen,
        kPoisonPeas,
        kBeetleEgg,
        kAzalea,
        kStick,
        kTringer,
        kMythicTringer,
        kTriweb,
        kAntennae,
        kTricac,
        kHeaviest,
        kThirdEye,
        kObserver,
        kPoisonCactus,
        kSalt,
        kUniqueBasic,
        kSquare,
        kMoon,
        kLotus,
        kCutter,
        kYinYang,
        kYggdrasil,
        kRice,
        kBone,
        kYucca,
        kCorn,
        // -------------------------------------------------------------------
        // Wave-system rarity expansion. Every "combat-relevant" base petal
        // gets all 7 rarity tiers so the rarity ramp during a round can
        // upgrade drop pools naturally. Existing IDs above are unchanged so
        // older trained checkpoints (which reference petals by integer)
        // continue to load without rewriting embedding rows. Stats for
        // each new entry are formula-scaled by petal_at_tier() in
        // StaticData.cc — HP × 1.6^Δ, damage × 1.5^Δ, healing /
        // poison × 1.5^Δ, with reload / count / radius held constant.
        kLegendaryBasic,
        kMythicBasic,
        kLegendaryLight,
        kMythicLight,
        kUniqueLight,
        kMythicHeavy,
        kUniqueHeavy,
        kUniqueStinger,
        kMythicLeaf,
        kUniqueLeaf,
        kCommonTwin,
        kRareTwin,
        kEpicTwin,
        kLegendaryTwin,
        kMythicTwin,
        kUniqueTwin,
        kMythicRose,
        kUniqueRose,
        kMythicIris,
        kUniqueIris,
        kMythicBubble,
        kUniqueBubble,
        kMythicRock,
        kUniqueRock,
        kCommonWeb,
        kUnusualWeb,
        kEpicWeb,
        kMythicWeb,
        kUniqueWeb,
        kCommonWing,
        kUnusualWing,
        kEpicWing,
        kLegendaryWing,
        kMythicWing,
        kUniqueWing,
        kCommonPeas,
        kUnusualPeas,
        kLegendaryPeas,
        kMythicPeas,
        kUniquePeas,
        kCommonSand,
        kUnusualSand,
        kEpicSand,
        kLegendarySand,
        kMythicSand,
        kUniqueSand,
        kCommonPincer,
        kUnusualPincer,
        kEpicPincer,
        kLegendaryPincer,
        kMythicPincer,
        kUniquePincer,
        kCommonDahlia,
        kUnusualDahlia,
        kEpicDahlia,
        kLegendaryDahlia,
        kMythicDahlia,
        kUniqueDahlia,
        kCommonTriplet,
        kUnusualTriplet,
        kRareTriplet,
        kLegendaryTriplet,
        kMythicTriplet,
        kUniqueTriplet,
        kCommonSalt,
        kUnusualSalt,
        kEpicSalt,
        kLegendarySalt,
        kMythicSalt,
        kUniqueSalt,
        kCommonPollen,
        kUnusualPollen,
        kRarePollen,
        kLegendaryPollen,
        kMythicPollen,
        kUniquePollen,
        kCommonFaster,
        kUnusualFaster,
        kEpicFaster,
        kLegendaryFaster,
        kMythicFaster,
        kUniqueFaster,
        kCommonCactus,
        kUnusualCactus,
        kMythicCactus,
        kUniqueCactus,
        kCommonMissile,
        kUnusualMissile,
        kEpicMissile,
        kLegendaryMissile,
        kMythicMissile,
        kUniqueMissile,
        kCommonDandelion,
        kUnusualDandelion,
        kEpicDandelion,
        kLegendaryDandelion,
        kMythicDandelion,
        kUniqueDandelion,
        kCommonYucca,
        kRareYucca,
        kEpicYucca,
        kLegendaryYucca,
        kMythicYucca,
        kUniqueYucca,
        kCommonBone,
        kUnusualBone,
        kRareBone,
        kEpicBone,
        kMythicBone,
        kUniqueBone,
        kCommonRice,
        kUnusualRice,
        kRareRice,
        kLegendaryRice,
        kMythicRice,
        kUniqueRice,
        kCommonCorn,
        kUnusualCorn,
        kRareCorn,
        kLegendaryCorn,
        kMythicCorn,
        kUniqueCorn,
        // Appended (not slotted next to other Peas tiers) so existing IDs
        // stay stable for trained checkpoints that key petals by integer.
        kEpicPeas,
        kNumPetals
    };
};

namespace MobID {
    typedef uint8_t T;
    enum : T {
        kBabyAnt,
        kWorkerAnt,
        kSoldierAnt,
        kBee,
        kLadybug,
        kBeetle,
        kMassiveLadybug,
        kMassiveBeetle,
        kDarkLadybug,
        kHornet,
        kCactus,
        kRock,
        kBoulder,
        kCentipede,
        kEvilCentipede,
        kDesertCentipede,
        kSandstorm,
        kScorpion,
        kSpider,
        kAntHole,
        kQueenAnt,
        kShinyLadybug,
        kSquare,
        kDigger,
        kLeafbug,
        kBush,
        kMantis,
        kWasp,
        kNumMobs
    };
};

namespace RarityID {
    enum {
        kCommon,
        kUnusual,
        kRare,
        kEpic,
        kLegendary,
        kMythic,
        kUnique,
        kNumRarities
    };
};

namespace ColorID {
    enum {
        kYellow,
        kGray,
        kBlue,
        kRed,
        kNumColors
    };
};

namespace AIState {
    enum {
        kIdle,
        kIdleMoving,
        kReturning,
        kBasicAggro
    };
};

namespace EntityFlags {
    enum {
        kIsDespawning,
        kNoFriendlyCollision,
        kDieOnParentDeath,
        kSpawnedFromZone,
        kNoDrops,
        kHasCulling,
        kIsCulled
    };
};

namespace FaceFlags {
    enum {
        kAttacking,
        kDefending,
        kPoisoned,
        kDandelioned,
        kThirdEye,
        kAntennae,
        kObserver,
        kCutter
    };
};

namespace InputFlags {
    enum {
        kAttacking,
        kDefending
    };
}

struct PoisonDamage {
    float damage;
    float time;
};

struct PetalAttributes {
    enum {
        kPassiveRot,
        kNoRot,
        kFollowRot
    };
    float clump_radius = 0;
    float secondary_reload = 0;
    float constant_heal = 0;
    float burst_heal = 0;
    float mass = 0.1;
    uint8_t defend_only = 0;
    float icon_angle = 0;
    uint8_t rotation_style = kPassiveRot;
    struct PoisonDamage poison_damage;
    uint8_t spawns = MobID::kNumMobs;
    uint8_t spawn_count = 0;
};

struct PetalData {
    char const *name;
    char const *description;
    float health;
    float damage;
    float radius;
    float reload;
    uint8_t count;
    uint8_t rarity;
    struct PetalAttributes attributes;
};

struct MobAttributes {
    float aggro_radius = 500;
    uint8_t segments = 1;
    uint8_t stationary;
    struct PoisonDamage poison_damage;
};

struct MobData {
    char const *name;
    char const *description;
    uint8_t rarity;
    RangeValue health;
    float damage;
    RangeValue radius;
    uint32_t xp;
    StaticArray<PetalID::T, MAX_DROPS_PER_MOB> drops;
    struct MobAttributes attributes;
};

struct SpawnChance {
    MobID::T id;
    float chance;
};

struct ZoneDefinition {
    float left;
    float top;
    float right;
    float bottom;
    float density;
    float drop_multiplier;
    StaticArray<struct SpawnChance, MobID::kNumMobs> spawns;
    uint32_t difficulty;
    uint32_t color;
    char const *name;
};
