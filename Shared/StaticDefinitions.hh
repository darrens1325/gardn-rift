#pragma once

#include <Shared/Helpers.hh>

#include <cstdint>

// Derived from the Tiled map at Map/main/main.tmj (width × tilewidth,
// height × tileheight) by Shared/MapDims.cmake at CMake configure time.
// The hardcoded MAP zones in StaticData.hh only cover the 0..20000
// quadrant; positions outside fall through `get_zone_from_pos` to zone 0
// and the Tiled spawn polygons populate them.
#include <Shared/MapDimensions.hh>

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

// Coordinate axis #1 of the (type, rarity) petal identity. Each value is a
// distinct mechanical "family" — different stat curves, attributes, on-hit
// effects. Two petals with the same `PetalType` differ only in their stat
// numbers (HP, damage, etc.) along the rarity axis. Variants that look
// similar but behave differently — single Stinger vs clump Tringer, Cactus
// vs Poison-Cactus, Iris vs Blue-Iris, ant-spawn Egg vs beetle-spawn Egg —
// each get their own type, so every (type, rarity) cell holds a unique petal.
namespace PetalType {
    typedef uint8_t T;
    enum : T {
        kNone,
        kBasic,
        kLight,
        kHeavy,
        kStinger,
        kTringer,
        kLeaf,
        kTwin,
        kRose,
        kAzalea,
        kIris,
        kBlueIris,
        kMissile,
        kDandelion,
        kBubble,
        kFaster,
        kRock,
        kCactus,
        kPoisonCactus,
        kTricac,
        kWeb,
        kTriweb,
        kWing,
        kPeas,
        kPoisonPeas,
        kSand,
        kPincer,
        kDahlia,
        kTriplet,
        kAntEgg,
        kBeetleEgg,
        kPollen,
        kStick,
        kAntennae,
        kHeaviest,
        kThirdEye,
        kObserver,
        kSalt,
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
        kRoot,
        kNumPetalTypes
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
        kUltra,
        kSuper,
        kUnique,
        kEternal,
        kNumRarities
    };
};

// Coordinate pair on the (type, rarity) plane. Wire encoding is two bytes:
// the type byte, then the rarity byte. Compares lexicographically so sorting
// by PetalID groups all rarities of a type together.
struct Petal {
    PetalType::T type;
    uint8_t      rarity;
    constexpr bool operator==(Petal const &) const = default;
    constexpr bool operator!=(Petal const &) const = default;
    constexpr auto operator<=>(Petal const &) const = default;
};

namespace PetalID {
    using T = Petal;

    // Legacy enumerated names. They keep the same call-site spelling
    // (`PetalID::kBasic`, `PetalID::kEpicLight`, …) but now resolve to the
    // (type, rarity) coordinate instead of a packed scalar. Drop-list arrays
    // and switch statements continue to work; the wire format and tables
    // are now indexed two-dimensionally.
    inline constexpr Petal kNone             = { PetalType::kNone,          RarityID::kCommon    };
    inline constexpr Petal kBasic            = { PetalType::kBasic,         RarityID::kCommon    };
    inline constexpr Petal kUnusualBasic     = { PetalType::kBasic,         RarityID::kUnusual   };
    inline constexpr Petal kRareBasic        = { PetalType::kBasic,         RarityID::kRare      };
    inline constexpr Petal kEpicBasic        = { PetalType::kBasic,         RarityID::kEpic      };
    inline constexpr Petal kLegendaryBasic   = { PetalType::kBasic,         RarityID::kLegendary };
    inline constexpr Petal kMythicBasic      = { PetalType::kBasic,         RarityID::kMythic    };
    inline constexpr Petal kUniqueBasic      = { PetalType::kBasic,         RarityID::kUnique    };
    inline constexpr Petal kLight            = { PetalType::kLight,         RarityID::kCommon    };
    inline constexpr Petal kUnusualLight     = { PetalType::kLight,         RarityID::kUnusual   };
    inline constexpr Petal kRareLight        = { PetalType::kLight,         RarityID::kRare      };
    inline constexpr Petal kEpicLight        = { PetalType::kLight,         RarityID::kEpic      };
    inline constexpr Petal kLegendaryLight   = { PetalType::kLight,         RarityID::kLegendary };
    inline constexpr Petal kMythicLight      = { PetalType::kLight,         RarityID::kMythic    };
    inline constexpr Petal kUniqueLight      = { PetalType::kLight,         RarityID::kUnique    };
    inline constexpr Petal kHeavy            = { PetalType::kHeavy,         RarityID::kCommon    };
    inline constexpr Petal kUnusualHeavy     = { PetalType::kHeavy,         RarityID::kUnusual   };
    inline constexpr Petal kRareHeavy        = { PetalType::kHeavy,         RarityID::kRare      };
    inline constexpr Petal kEpicHeavy        = { PetalType::kHeavy,         RarityID::kEpic      };
    inline constexpr Petal kLegendaryHeavy   = { PetalType::kHeavy,         RarityID::kLegendary };
    inline constexpr Petal kMythicHeavy      = { PetalType::kHeavy,         RarityID::kMythic    };
    inline constexpr Petal kUniqueHeavy      = { PetalType::kHeavy,         RarityID::kUnique    };
    inline constexpr Petal kCommonStinger    = { PetalType::kStinger,       RarityID::kCommon    };
    inline constexpr Petal kStinger          = { PetalType::kStinger,       RarityID::kUnusual   };
    inline constexpr Petal kRareStinger      = { PetalType::kStinger,       RarityID::kRare      };
    inline constexpr Petal kEpicStinger      = { PetalType::kStinger,       RarityID::kEpic      };
    inline constexpr Petal kTringer          = { PetalType::kTringer,       RarityID::kLegendary };
    inline constexpr Petal kMythicTringer    = { PetalType::kTringer,       RarityID::kMythic    };
    inline constexpr Petal kUniqueStinger    = { PetalType::kTringer,       RarityID::kUnique    };
    inline constexpr Petal kCommonLeaf       = { PetalType::kLeaf,          RarityID::kCommon    };
    inline constexpr Petal kLeaf             = { PetalType::kLeaf,          RarityID::kUnusual   };
    inline constexpr Petal kRareLeaf         = { PetalType::kLeaf,          RarityID::kRare      };
    inline constexpr Petal kEpicLeaf         = { PetalType::kLeaf,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryLeaf    = { PetalType::kLeaf,          RarityID::kLegendary };
    inline constexpr Petal kMythicLeaf       = { PetalType::kLeaf,          RarityID::kMythic    };
    inline constexpr Petal kUniqueLeaf       = { PetalType::kLeaf,          RarityID::kUnique    };
    inline constexpr Petal kCommonTwin       = { PetalType::kTwin,          RarityID::kCommon    };
    inline constexpr Petal kTwin             = { PetalType::kTwin,          RarityID::kUnusual   };
    inline constexpr Petal kRareTwin         = { PetalType::kTwin,          RarityID::kRare      };
    inline constexpr Petal kEpicTwin         = { PetalType::kTwin,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryTwin    = { PetalType::kTwin,          RarityID::kLegendary };
    inline constexpr Petal kMythicTwin       = { PetalType::kTwin,          RarityID::kMythic    };
    inline constexpr Petal kUniqueTwin       = { PetalType::kTwin,          RarityID::kUnique    };
    inline constexpr Petal kCommonRose       = { PetalType::kRose,          RarityID::kCommon    };
    inline constexpr Petal kRose             = { PetalType::kRose,          RarityID::kUnusual   };
    inline constexpr Petal kRareRose         = { PetalType::kRose,          RarityID::kRare      };
    inline constexpr Petal kEpicRose         = { PetalType::kRose,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryRose    = { PetalType::kRose,          RarityID::kLegendary };
    inline constexpr Petal kMythicRose       = { PetalType::kRose,          RarityID::kMythic    };
    inline constexpr Petal kUniqueRose       = { PetalType::kRose,          RarityID::kUnique    };
    inline constexpr Petal kAzalea           = { PetalType::kAzalea,        RarityID::kEpic      };
    inline constexpr Petal kCommonIris       = { PetalType::kIris,          RarityID::kCommon    };
    inline constexpr Petal kIris             = { PetalType::kIris,          RarityID::kUnusual   };
    inline constexpr Petal kRareIris         = { PetalType::kIris,          RarityID::kRare      };
    inline constexpr Petal kLegendaryIris    = { PetalType::kIris,          RarityID::kLegendary };
    inline constexpr Petal kMythicIris       = { PetalType::kIris,          RarityID::kMythic    };
    inline constexpr Petal kUniqueIris       = { PetalType::kIris,          RarityID::kUnique    };
    inline constexpr Petal kBlueIris         = { PetalType::kBlueIris,      RarityID::kEpic      };
    inline constexpr Petal kCommonMissile    = { PetalType::kMissile,       RarityID::kCommon    };
    inline constexpr Petal kUnusualMissile   = { PetalType::kMissile,       RarityID::kUnusual   };
    inline constexpr Petal kMissile          = { PetalType::kMissile,       RarityID::kRare      };
    inline constexpr Petal kEpicMissile      = { PetalType::kMissile,       RarityID::kEpic      };
    inline constexpr Petal kLegendaryMissile = { PetalType::kMissile,       RarityID::kLegendary };
    inline constexpr Petal kMythicMissile    = { PetalType::kMissile,       RarityID::kMythic    };
    inline constexpr Petal kUniqueMissile    = { PetalType::kMissile,       RarityID::kUnique    };
    inline constexpr Petal kCommonDandelion  = { PetalType::kDandelion,     RarityID::kCommon    };
    inline constexpr Petal kUnusualDandelion = { PetalType::kDandelion,     RarityID::kUnusual   };
    inline constexpr Petal kDandelion        = { PetalType::kDandelion,     RarityID::kRare      };
    inline constexpr Petal kEpicDandelion    = { PetalType::kDandelion,     RarityID::kEpic      };
    inline constexpr Petal kLegendaryDandelion={ PetalType::kDandelion,     RarityID::kLegendary };
    inline constexpr Petal kMythicDandelion  = { PetalType::kDandelion,     RarityID::kMythic    };
    inline constexpr Petal kUniqueDandelion  = { PetalType::kDandelion,     RarityID::kUnique    };
    inline constexpr Petal kCommonBubble     = { PetalType::kBubble,        RarityID::kCommon    };
    inline constexpr Petal kUnusualBubble    = { PetalType::kBubble,        RarityID::kUnusual   };
    inline constexpr Petal kBubble           = { PetalType::kBubble,        RarityID::kRare      };
    inline constexpr Petal kEpicBubble       = { PetalType::kBubble,        RarityID::kEpic      };
    inline constexpr Petal kLegendaryBubble  = { PetalType::kBubble,        RarityID::kLegendary };
    inline constexpr Petal kMythicBubble     = { PetalType::kBubble,        RarityID::kMythic    };
    inline constexpr Petal kUniqueBubble     = { PetalType::kBubble,        RarityID::kUnique    };
    inline constexpr Petal kCommonFaster     = { PetalType::kFaster,        RarityID::kCommon    };
    inline constexpr Petal kUnusualFaster    = { PetalType::kFaster,        RarityID::kUnusual   };
    inline constexpr Petal kFaster           = { PetalType::kFaster,        RarityID::kRare      };
    inline constexpr Petal kEpicFaster       = { PetalType::kFaster,        RarityID::kEpic      };
    inline constexpr Petal kLegendaryFaster  = { PetalType::kFaster,        RarityID::kLegendary };
    inline constexpr Petal kMythicFaster     = { PetalType::kFaster,        RarityID::kMythic    };
    inline constexpr Petal kUniqueFaster     = { PetalType::kFaster,        RarityID::kUnique    };
    inline constexpr Petal kCommonRock       = { PetalType::kRock,          RarityID::kCommon    };
    inline constexpr Petal kUnusualRock      = { PetalType::kRock,          RarityID::kUnusual   };
    inline constexpr Petal kRock             = { PetalType::kRock,          RarityID::kRare      };
    inline constexpr Petal kEpicRock         = { PetalType::kRock,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryRock    = { PetalType::kRock,          RarityID::kLegendary };
    inline constexpr Petal kMythicRock       = { PetalType::kRock,          RarityID::kMythic    };
    inline constexpr Petal kUniqueRock       = { PetalType::kRock,          RarityID::kUnique    };
    inline constexpr Petal kCommonCactus     = { PetalType::kCactus,        RarityID::kCommon    };
    inline constexpr Petal kUnusualCactus    = { PetalType::kCactus,        RarityID::kUnusual   };
    inline constexpr Petal kCactus           = { PetalType::kCactus,        RarityID::kRare      };
    inline constexpr Petal kMythicCactus     = { PetalType::kCactus,        RarityID::kMythic    };
    inline constexpr Petal kUniqueCactus     = { PetalType::kCactus,        RarityID::kUnique    };
    inline constexpr Petal kPoisonCactus     = { PetalType::kPoisonCactus,  RarityID::kEpic      };
    inline constexpr Petal kTricac           = { PetalType::kTricac,        RarityID::kLegendary };
    inline constexpr Petal kCommonWeb        = { PetalType::kWeb,           RarityID::kCommon    };
    inline constexpr Petal kUnusualWeb       = { PetalType::kWeb,           RarityID::kUnusual   };
    inline constexpr Petal kWeb              = { PetalType::kWeb,           RarityID::kRare      };
    inline constexpr Petal kEpicWeb          = { PetalType::kWeb,           RarityID::kEpic      };
    inline constexpr Petal kTriweb           = { PetalType::kTriweb,        RarityID::kLegendary };
    inline constexpr Petal kMythicWeb        = { PetalType::kTriweb,        RarityID::kMythic    };
    inline constexpr Petal kUniqueWeb        = { PetalType::kTriweb,        RarityID::kUnique    };
    inline constexpr Petal kCommonWing       = { PetalType::kWing,          RarityID::kCommon    };
    inline constexpr Petal kUnusualWing      = { PetalType::kWing,          RarityID::kUnusual   };
    inline constexpr Petal kWing             = { PetalType::kWing,          RarityID::kRare      };
    inline constexpr Petal kEpicWing         = { PetalType::kWing,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryWing    = { PetalType::kWing,          RarityID::kLegendary };
    inline constexpr Petal kMythicWing       = { PetalType::kWing,          RarityID::kMythic    };
    inline constexpr Petal kUniqueWing       = { PetalType::kWing,          RarityID::kUnique    };
    inline constexpr Petal kCommonPeas       = { PetalType::kPeas,          RarityID::kCommon    };
    inline constexpr Petal kUnusualPeas      = { PetalType::kPeas,          RarityID::kUnusual   };
    inline constexpr Petal kPeas             = { PetalType::kPeas,          RarityID::kRare      };
    inline constexpr Petal kEpicPeas         = { PetalType::kPeas,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryPeas    = { PetalType::kPeas,          RarityID::kLegendary };
    inline constexpr Petal kMythicPeas       = { PetalType::kPeas,          RarityID::kMythic    };
    inline constexpr Petal kUniquePeas       = { PetalType::kPeas,          RarityID::kUnique    };
    inline constexpr Petal kPoisonPeas       = { PetalType::kPoisonPeas,    RarityID::kEpic      };
    inline constexpr Petal kCommonSand       = { PetalType::kSand,          RarityID::kCommon    };
    inline constexpr Petal kUnusualSand      = { PetalType::kSand,          RarityID::kUnusual   };
    inline constexpr Petal kSand             = { PetalType::kSand,          RarityID::kRare      };
    inline constexpr Petal kEpicSand         = { PetalType::kSand,          RarityID::kEpic      };
    inline constexpr Petal kLegendarySand    = { PetalType::kSand,          RarityID::kLegendary };
    inline constexpr Petal kMythicSand       = { PetalType::kSand,          RarityID::kMythic    };
    inline constexpr Petal kUniqueSand       = { PetalType::kSand,          RarityID::kUnique    };
    inline constexpr Petal kCommonPincer     = { PetalType::kPincer,        RarityID::kCommon    };
    inline constexpr Petal kUnusualPincer    = { PetalType::kPincer,        RarityID::kUnusual   };
    inline constexpr Petal kPincer           = { PetalType::kPincer,        RarityID::kRare      };
    inline constexpr Petal kEpicPincer       = { PetalType::kPincer,        RarityID::kEpic      };
    inline constexpr Petal kLegendaryPincer  = { PetalType::kPincer,        RarityID::kLegendary };
    inline constexpr Petal kMythicPincer     = { PetalType::kPincer,        RarityID::kMythic    };
    inline constexpr Petal kUniquePincer     = { PetalType::kPincer,        RarityID::kUnique    };
    inline constexpr Petal kCommonDahlia     = { PetalType::kDahlia,        RarityID::kCommon    };
    inline constexpr Petal kUnusualDahlia    = { PetalType::kDahlia,        RarityID::kUnusual   };
    inline constexpr Petal kDahlia           = { PetalType::kDahlia,        RarityID::kRare      };
    inline constexpr Petal kEpicDahlia       = { PetalType::kDahlia,        RarityID::kEpic      };
    inline constexpr Petal kLegendaryDahlia  = { PetalType::kDahlia,        RarityID::kLegendary };
    inline constexpr Petal kMythicDahlia     = { PetalType::kDahlia,        RarityID::kMythic    };
    inline constexpr Petal kUniqueDahlia     = { PetalType::kDahlia,        RarityID::kUnique    };
    inline constexpr Petal kCommonTriplet    = { PetalType::kTriplet,       RarityID::kCommon    };
    inline constexpr Petal kUnusualTriplet   = { PetalType::kTriplet,       RarityID::kUnusual   };
    inline constexpr Petal kRareTriplet      = { PetalType::kTriplet,       RarityID::kRare      };
    inline constexpr Petal kTriplet          = { PetalType::kTriplet,       RarityID::kEpic      };
    inline constexpr Petal kLegendaryTriplet = { PetalType::kTriplet,       RarityID::kLegendary };
    inline constexpr Petal kMythicTriplet    = { PetalType::kTriplet,       RarityID::kMythic    };
    inline constexpr Petal kUniqueTriplet    = { PetalType::kTriplet,       RarityID::kUnique    };
    inline constexpr Petal kAntEgg           = { PetalType::kAntEgg,        RarityID::kEpic      };
    inline constexpr Petal kBeetleEgg        = { PetalType::kBeetleEgg,     RarityID::kEpic      };
    inline constexpr Petal kCommonPollen     = { PetalType::kPollen,        RarityID::kCommon    };
    inline constexpr Petal kUnusualPollen    = { PetalType::kPollen,        RarityID::kUnusual   };
    inline constexpr Petal kRarePollen       = { PetalType::kPollen,        RarityID::kRare      };
    inline constexpr Petal kPollen           = { PetalType::kPollen,        RarityID::kEpic      };
    inline constexpr Petal kLegendaryPollen  = { PetalType::kPollen,        RarityID::kLegendary };
    inline constexpr Petal kMythicPollen     = { PetalType::kPollen,        RarityID::kMythic    };
    inline constexpr Petal kUniquePollen     = { PetalType::kPollen,        RarityID::kUnique    };
    inline constexpr Petal kStick            = { PetalType::kStick,         RarityID::kLegendary };
    inline constexpr Petal kAntennae         = { PetalType::kAntennae,      RarityID::kLegendary };
    inline constexpr Petal kHeaviest         = { PetalType::kHeaviest,      RarityID::kEpic      };
    inline constexpr Petal kThirdEye         = { PetalType::kThirdEye,      RarityID::kMythic    };
    inline constexpr Petal kObserver         = { PetalType::kObserver,      RarityID::kMythic    };
    inline constexpr Petal kCommonSalt       = { PetalType::kSalt,          RarityID::kCommon    };
    inline constexpr Petal kUnusualSalt      = { PetalType::kSalt,          RarityID::kUnusual   };
    inline constexpr Petal kSalt             = { PetalType::kSalt,          RarityID::kRare      };
    inline constexpr Petal kEpicSalt         = { PetalType::kSalt,          RarityID::kEpic      };
    inline constexpr Petal kLegendarySalt    = { PetalType::kSalt,          RarityID::kLegendary };
    inline constexpr Petal kMythicSalt       = { PetalType::kSalt,          RarityID::kMythic    };
    inline constexpr Petal kUniqueSalt       = { PetalType::kSalt,          RarityID::kUnique    };
    inline constexpr Petal kSquare           = { PetalType::kSquare,        RarityID::kUnique    };
    inline constexpr Petal kMoon             = { PetalType::kMoon,          RarityID::kMythic    };
    inline constexpr Petal kLotus            = { PetalType::kLotus,         RarityID::kEpic      };
    inline constexpr Petal kCutter           = { PetalType::kCutter,        RarityID::kEpic      };
    inline constexpr Petal kYinYang          = { PetalType::kYinYang,       RarityID::kEpic      };
    inline constexpr Petal kCommonYggdrasil  = { PetalType::kYggdrasil,     RarityID::kCommon    };
    inline constexpr Petal kUnusualYggdrasil = { PetalType::kYggdrasil,     RarityID::kUnusual   };
    inline constexpr Petal kRareYggdrasil    = { PetalType::kYggdrasil,     RarityID::kRare      };
    inline constexpr Petal kEpicYggdrasil    = { PetalType::kYggdrasil,     RarityID::kEpic      };
    inline constexpr Petal kLegendaryYggdrasil={ PetalType::kYggdrasil,     RarityID::kLegendary };
    inline constexpr Petal kMythicYggdrasil  = { PetalType::kYggdrasil,     RarityID::kMythic    };
    inline constexpr Petal kYggdrasil        = { PetalType::kYggdrasil,     RarityID::kUnique    };
    inline constexpr Petal kCommonRice       = { PetalType::kRice,          RarityID::kCommon    };
    inline constexpr Petal kUnusualRice      = { PetalType::kRice,          RarityID::kUnusual   };
    inline constexpr Petal kRareRice         = { PetalType::kRice,          RarityID::kRare      };
    inline constexpr Petal kRice             = { PetalType::kRice,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryRice    = { PetalType::kRice,          RarityID::kLegendary };
    inline constexpr Petal kMythicRice       = { PetalType::kRice,          RarityID::kMythic    };
    inline constexpr Petal kUniqueRice       = { PetalType::kRice,          RarityID::kUnique    };
    inline constexpr Petal kCommonBone       = { PetalType::kBone,          RarityID::kCommon    };
    inline constexpr Petal kUnusualBone      = { PetalType::kBone,          RarityID::kUnusual   };
    inline constexpr Petal kRareBone         = { PetalType::kBone,          RarityID::kRare      };
    inline constexpr Petal kEpicBone         = { PetalType::kBone,          RarityID::kEpic      };
    inline constexpr Petal kBone             = { PetalType::kBone,          RarityID::kLegendary };
    inline constexpr Petal kMythicBone       = { PetalType::kBone,          RarityID::kMythic    };
    inline constexpr Petal kUniqueBone       = { PetalType::kBone,          RarityID::kUnique    };
    inline constexpr Petal kCommonYucca      = { PetalType::kYucca,         RarityID::kCommon    };
    inline constexpr Petal kYucca            = { PetalType::kYucca,         RarityID::kUnusual   };
    inline constexpr Petal kRareYucca        = { PetalType::kYucca,         RarityID::kRare      };
    inline constexpr Petal kEpicYucca        = { PetalType::kYucca,         RarityID::kEpic      };
    inline constexpr Petal kLegendaryYucca   = { PetalType::kYucca,         RarityID::kLegendary };
    inline constexpr Petal kMythicYucca      = { PetalType::kYucca,         RarityID::kMythic    };
    inline constexpr Petal kUniqueYucca      = { PetalType::kYucca,         RarityID::kUnique    };
    inline constexpr Petal kCommonCorn       = { PetalType::kCorn,          RarityID::kCommon    };
    inline constexpr Petal kUnusualCorn      = { PetalType::kCorn,          RarityID::kUnusual   };
    inline constexpr Petal kRareCorn         = { PetalType::kCorn,          RarityID::kRare      };
    inline constexpr Petal kCorn             = { PetalType::kCorn,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryCorn    = { PetalType::kCorn,          RarityID::kLegendary };
    inline constexpr Petal kMythicCorn       = { PetalType::kCorn,          RarityID::kMythic    };
    inline constexpr Petal kUniqueCorn       = { PetalType::kCorn,          RarityID::kUnique    };
    inline constexpr Petal kCommonRoot       = { PetalType::kRoot,          RarityID::kCommon    };
    inline constexpr Petal kUnusualRoot      = { PetalType::kRoot,          RarityID::kUnusual   };
    inline constexpr Petal kRoot             = { PetalType::kRoot,          RarityID::kRare      };
    inline constexpr Petal kEpicRoot         = { PetalType::kRoot,          RarityID::kEpic      };
    inline constexpr Petal kLegendaryRoot    = { PetalType::kRoot,          RarityID::kLegendary };
    inline constexpr Petal kMythicRoot       = { PetalType::kRoot,          RarityID::kMythic    };
    inline constexpr Petal kUniqueRoot       = { PetalType::kRoot,          RarityID::kUnique    };
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

// Per-(type, rarity) cell data. `name`/`description`/numbers vary along the
// rarity axis; the type & rarity themselves are positional in PETAL_DATA
// and live on the Petal pair, not duplicated in here. A `nullptr` name
// marks an unauthored cell (e.g. PETAL_DATA[kBasic][kRare] before a Rare
// Basic existed) so consumers can detect "no petal at this coordinate."
struct PetalData {
    char const *name;
    char const *description;
    float health;
    float damage;
    float radius;
    float reload;
    uint8_t count;
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
