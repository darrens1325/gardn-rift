"""Binary protocol for the gardn server.

Mirrors Shared/Binary.cc and Shared/Entity.cc:
  - Unsigned ints use a 7-bit varint (LEB128-style) with the MSB as continuation.
  - Signed ints zig-zag (sign in low bit, magnitude shifted up by 1).
  - Floats are an int64 of (value * 64) — server uses int 1/64ths on the wire.
  - EntityID is u16(varint) id, then u8 hash if id != 0.
"""

from __future__ import annotations

PROTOCOL_FLOAT_SCALE = 64

# Serverbound
S_VERIFY = 0
S_CLIENT_INPUT = 1
S_CLIENT_SPAWN = 2
S_PETAL_SWAP = 3
S_PETAL_DELETE = 4
S_CLIENT_CHAT = 5     # { u8 op, string text }; server validates + rebroadcasts
# { u8: S_STEP } — sync-mode only. Each verified client raises a
# "ready for next tick" flag on the server; when every verified client
# has raised it, the server runs exactly one tick and clears the flags.
# A no-op in wall-clock mode (server doesn't enter sync_mode unless
# launched with GARDN_SYNC=1).
S_STEP = 6

# Inventory layout (Shared/EntityDef.hh): loadout_ids has 2 * MAX_SLOT_COUNT
# entries. The first `loadout_count` are the active orbital slots (loadout
# slot count grows with level — 5 at level 1, see loadout_slots_at_level).
# Slots [loadout_count, loadout_count + MAX_SLOT_COUNT) are storage. The
# server auto-deposits drops into the first kNone slot it finds across the
# whole array, so storage tends to fill before any swap is performed.
MAX_SLOT_COUNT = 8

# Rarity ranks, matching RarityID in Shared/StaticDefinitions.hh. Higher is
# strictly better for our equip-priority decision.
RARITY_COMMON    = 0
RARITY_UNUSUAL   = 1
RARITY_RARE      = 2
RARITY_EPIC      = 3
RARITY_LEGENDARY = 4
RARITY_MYTHIC    = 5
RARITY_UNIQUE    = 6

# PetalID -> rarity. Index matches PetalID::T enum order in
# Shared/StaticDefinitions.hh; values come from PETAL_DATA in
# Shared/StaticData.cc. kNone is a sentinel and is treated as the worst rank
# (the inventory manager filters it through `PETAL_NONE` before consulting
# this table). When new rarity tiers are added to a petal, both the enum and
# this table need a new entry — keep the order in lock-step with PETAL_DATA.
PETAL_RARITY = [
    RARITY_COMMON,     #  0  kNone (sentinel)
    RARITY_COMMON,     #  1  kBasic
    RARITY_UNUSUAL,    #  2  kUnusualBasic
    RARITY_RARE,       #  3  kRareBasic
    RARITY_EPIC,       #  4  kEpicBasic
    RARITY_COMMON,     #  5  kLight
    RARITY_UNUSUAL,    #  6  kUnusualLight
    RARITY_RARE,       #  7  kRareLight
    RARITY_EPIC,       #  8  kEpicLight
    RARITY_COMMON,     #  9  kHeavy
    RARITY_UNUSUAL,    # 10  kUnusualHeavy
    RARITY_RARE,       # 11  kRareHeavy
    RARITY_EPIC,       # 12  kEpicHeavy
    RARITY_LEGENDARY,  # 13  kLegendaryHeavy
    RARITY_COMMON,     # 14  kCommonStinger
    RARITY_UNUSUAL,    # 15  kStinger
    RARITY_RARE,       # 16  kRareStinger
    RARITY_EPIC,       # 17  kEpicStinger
    RARITY_COMMON,     # 18  kCommonLeaf
    RARITY_UNUSUAL,    # 19  kLeaf
    RARITY_RARE,       # 20  kRareLeaf
    RARITY_EPIC,       # 21  kEpicLeaf
    RARITY_LEGENDARY,  # 22  kLegendaryLeaf
    RARITY_UNUSUAL,    # 23  kTwin
    RARITY_COMMON,     # 24  kCommonRose
    RARITY_UNUSUAL,    # 25  kRose
    RARITY_RARE,       # 26  kRareRose
    RARITY_LEGENDARY,  # 27  kLegendaryRose
    RARITY_COMMON,     # 28  kCommonIris
    RARITY_UNUSUAL,    # 29  kIris
    RARITY_RARE,       # 30  kRareIris
    RARITY_LEGENDARY,  # 31  kLegendaryIris
    RARITY_RARE,       # 32  kMissile
    RARITY_RARE,       # 33  kDandelion
    RARITY_COMMON,     # 34  kCommonBubble
    RARITY_UNUSUAL,    # 35  kUnusualBubble
    RARITY_RARE,       # 36  kBubble
    RARITY_EPIC,       # 37  kEpicBubble
    RARITY_LEGENDARY,  # 38  kLegendaryBubble
    RARITY_RARE,       # 39  kFaster
    RARITY_COMMON,     # 40  kCommonRock
    RARITY_UNUSUAL,    # 41  kUnusualRock
    RARITY_RARE,       # 42  kRock
    RARITY_EPIC,       # 43  kEpicRock
    RARITY_LEGENDARY,  # 44  kLegendaryRock
    RARITY_RARE,       # 45  kCactus
    RARITY_RARE,       # 46  kWeb
    RARITY_RARE,       # 47  kWing
    RARITY_RARE,       # 48  kPeas
    RARITY_RARE,       # 49  kSand
    RARITY_RARE,       # 50  kPincer
    RARITY_RARE,       # 51  kDahlia
    RARITY_EPIC,       # 52  kTriplet
    RARITY_EPIC,       # 53  kAntEgg
    RARITY_EPIC,       # 54  kBlueIris
    RARITY_EPIC,       # 55  kPollen
    RARITY_EPIC,       # 56  kPoisonPeas
    RARITY_EPIC,       # 57  kBeetleEgg
    RARITY_EPIC,       # 58  kAzalea
    RARITY_LEGENDARY,  # 59  kStick
    RARITY_LEGENDARY,  # 60  kTringer
    RARITY_MYTHIC,     # 61  kMythicTringer
    RARITY_LEGENDARY,  # 62  kTriweb
    RARITY_LEGENDARY,  # 63  kAntennae
    RARITY_LEGENDARY,  # 64  kTricac
    RARITY_EPIC,       # 65  kHeaviest
    RARITY_MYTHIC,     # 66  kThirdEye
    RARITY_MYTHIC,     # 67  kObserver
    RARITY_EPIC,       # 68  kPoisonCactus
    RARITY_RARE,       # 69  kSalt
    RARITY_UNIQUE,     # 70  kUniqueBasic
    RARITY_UNIQUE,     # 71  kSquare
    RARITY_MYTHIC,     # 72  kMoon
    RARITY_EPIC,       # 73  kLotus
    RARITY_EPIC,       # 74  kCutter
    RARITY_EPIC,       # 75  kYinYang
    RARITY_UNIQUE,     # 76  kYggdrasil
    RARITY_EPIC,       # 77  kRice
    RARITY_LEGENDARY,  # 78  kBone
    RARITY_UNUSUAL,    # 79  kYucca
    RARITY_EPIC,       # 80  kCorn
    # ---- wave-system rarity expansion (Shared/StaticDefinitions.hh) ----
    RARITY_LEGENDARY,  # 81  kLegendaryBasic
    RARITY_MYTHIC,     # 82  kMythicBasic
    RARITY_LEGENDARY,  # 83  kLegendaryLight
    RARITY_MYTHIC,     # 84  kMythicLight
    RARITY_UNIQUE,     # 85  kUniqueLight
    RARITY_MYTHIC,     # 86  kMythicHeavy
    RARITY_UNIQUE,     # 87  kUniqueHeavy
    RARITY_UNIQUE,     # 88  kUniqueStinger
    RARITY_MYTHIC,     # 89  kMythicLeaf
    RARITY_UNIQUE,     # 90  kUniqueLeaf
    RARITY_COMMON,     # 91  kCommonTwin
    RARITY_RARE,       # 92  kRareTwin
    RARITY_EPIC,       # 93  kEpicTwin
    RARITY_LEGENDARY,  # 94  kLegendaryTwin
    RARITY_MYTHIC,     # 95  kMythicTwin
    RARITY_UNIQUE,     # 96  kUniqueTwin
    RARITY_MYTHIC,     # 97  kMythicRose
    RARITY_UNIQUE,     # 98  kUniqueRose
    RARITY_MYTHIC,     # 99  kMythicIris
    RARITY_UNIQUE,     # 100 kUniqueIris
    RARITY_MYTHIC,     # 101 kMythicBubble
    RARITY_UNIQUE,     # 102 kUniqueBubble
    RARITY_MYTHIC,     # 103 kMythicRock
    RARITY_UNIQUE,     # 104 kUniqueRock
    RARITY_COMMON,     # 105 kCommonWeb
    RARITY_UNUSUAL,    # 106 kUnusualWeb
    RARITY_EPIC,       # 107 kEpicWeb
    RARITY_MYTHIC,     # 108 kMythicWeb
    RARITY_UNIQUE,     # 109 kUniqueWeb
    RARITY_COMMON,     # 110 kCommonWing
    RARITY_UNUSUAL,    # 111 kUnusualWing
    RARITY_EPIC,       # 112 kEpicWing
    RARITY_LEGENDARY,  # 113 kLegendaryWing
    RARITY_MYTHIC,     # 114 kMythicWing
    RARITY_UNIQUE,     # 115 kUniqueWing
    RARITY_COMMON,     # 116 kCommonPeas
    RARITY_UNUSUAL,    # 117 kUnusualPeas
    RARITY_LEGENDARY,  # 118 kLegendaryPeas
    RARITY_MYTHIC,     # 119 kMythicPeas
    RARITY_UNIQUE,     # 120 kUniquePeas
    RARITY_COMMON,     # 121 kCommonSand
    RARITY_UNUSUAL,    # 122 kUnusualSand
    RARITY_EPIC,       # 123 kEpicSand
    RARITY_LEGENDARY,  # 124 kLegendarySand
    RARITY_MYTHIC,     # 125 kMythicSand
    RARITY_UNIQUE,     # 126 kUniqueSand
    RARITY_COMMON,     # 127 kCommonPincer
    RARITY_UNUSUAL,    # 128 kUnusualPincer
    RARITY_EPIC,       # 129 kEpicPincer
    RARITY_LEGENDARY,  # 130 kLegendaryPincer
    RARITY_MYTHIC,     # 131 kMythicPincer
    RARITY_UNIQUE,     # 132 kUniquePincer
    RARITY_COMMON,     # 133 kCommonDahlia
    RARITY_UNUSUAL,    # 134 kUnusualDahlia
    RARITY_EPIC,       # 135 kEpicDahlia
    RARITY_LEGENDARY,  # 136 kLegendaryDahlia
    RARITY_MYTHIC,     # 137 kMythicDahlia
    RARITY_UNIQUE,     # 138 kUniqueDahlia
    RARITY_COMMON,     # 139 kCommonTriplet
    RARITY_UNUSUAL,    # 140 kUnusualTriplet
    RARITY_RARE,       # 141 kRareTriplet
    RARITY_LEGENDARY,  # 142 kLegendaryTriplet
    RARITY_MYTHIC,     # 143 kMythicTriplet
    RARITY_UNIQUE,     # 144 kUniqueTriplet
    RARITY_COMMON,     # 145 kCommonSalt
    RARITY_UNUSUAL,    # 146 kUnusualSalt
    RARITY_EPIC,       # 147 kEpicSalt
    RARITY_LEGENDARY,  # 148 kLegendarySalt
    RARITY_MYTHIC,     # 149 kMythicSalt
    RARITY_UNIQUE,     # 150 kUniqueSalt
    RARITY_COMMON,     # 151 kCommonPollen
    RARITY_UNUSUAL,    # 152 kUnusualPollen
    RARITY_RARE,       # 153 kRarePollen
    RARITY_LEGENDARY,  # 154 kLegendaryPollen
    RARITY_MYTHIC,     # 155 kMythicPollen
    RARITY_UNIQUE,     # 156 kUniquePollen
    RARITY_COMMON,     # 157 kCommonFaster
    RARITY_UNUSUAL,    # 158 kUnusualFaster
    RARITY_EPIC,       # 159 kEpicFaster
    RARITY_LEGENDARY,  # 160 kLegendaryFaster
    RARITY_MYTHIC,     # 161 kMythicFaster
    RARITY_UNIQUE,     # 162 kUniqueFaster
    RARITY_COMMON,     # 163 kCommonCactus
    RARITY_UNUSUAL,    # 164 kUnusualCactus
    RARITY_MYTHIC,     # 165 kMythicCactus
    RARITY_UNIQUE,     # 166 kUniqueCactus
    RARITY_COMMON,     # 167 kCommonMissile
    RARITY_UNUSUAL,    # 168 kUnusualMissile
    RARITY_EPIC,       # 169 kEpicMissile
    RARITY_LEGENDARY,  # 170 kLegendaryMissile
    RARITY_MYTHIC,     # 171 kMythicMissile
    RARITY_UNIQUE,     # 172 kUniqueMissile
    RARITY_COMMON,     # 173 kCommonDandelion
    RARITY_UNUSUAL,    # 174 kUnusualDandelion
    RARITY_EPIC,       # 175 kEpicDandelion
    RARITY_LEGENDARY,  # 176 kLegendaryDandelion
    RARITY_MYTHIC,     # 177 kMythicDandelion
    RARITY_UNIQUE,     # 178 kUniqueDandelion
    RARITY_COMMON,     # 179 kCommonYucca
    RARITY_RARE,       # 180 kRareYucca
    RARITY_EPIC,       # 181 kEpicYucca
    RARITY_LEGENDARY,  # 182 kLegendaryYucca
    RARITY_MYTHIC,     # 183 kMythicYucca
    RARITY_UNIQUE,     # 184 kUniqueYucca
    RARITY_COMMON,     # 185 kCommonBone
    RARITY_UNUSUAL,    # 186 kUnusualBone
    RARITY_RARE,       # 187 kRareBone
    RARITY_EPIC,       # 188 kEpicBone
    RARITY_MYTHIC,     # 189 kMythicBone
    RARITY_UNIQUE,     # 190 kUniqueBone
    RARITY_COMMON,     # 191 kCommonRice
    RARITY_UNUSUAL,    # 192 kUnusualRice
    RARITY_RARE,       # 193 kRareRice
    RARITY_LEGENDARY,  # 194 kLegendaryRice
    RARITY_MYTHIC,     # 195 kMythicRice
    RARITY_UNIQUE,     # 196 kUniqueRice
    RARITY_COMMON,     # 197 kCommonCorn
    RARITY_UNUSUAL,    # 198 kUnusualCorn
    RARITY_RARE,       # 199 kRareCorn
    RARITY_LEGENDARY,  # 200 kLegendaryCorn
    RARITY_MYTHIC,     # 201 kMythicCorn
    RARITY_UNIQUE,     # 202 kUniqueCorn
    # ---- post-expansion appends (kept at the tail to preserve all IDs
    # below for trained checkpoints) ----
    RARITY_EPIC,       # 203 kEpicPeas
    RARITY_COMMON,     # 204 kCommonRoot
    RARITY_UNUSUAL,    # 205 kUnusualRoot
    RARITY_RARE,       # 206 kRoot
    RARITY_EPIC,       # 207 kEpicRoot
    RARITY_LEGENDARY,  # 208 kLegendaryRoot
    RARITY_MYTHIC,     # 209 kMythicRoot
    RARITY_UNIQUE,     # 210 kUniqueRoot
]

PETAL_NONE = 0
PETAL_BASIC = 1

# Coarse petal *role* taxonomy. Lets the policy distinguish "this is a
# healer" from "this is a damage petal" — rarity alone says nothing about
# behaviour. Categories are intentionally small (5 + empty) so a single
# scalar per slot keeps the state vector compact while still giving the
# network the signal it needs to make role-aware swap/delete decisions.
PETAL_TYPE_NONE     = 0   # empty slot
PETAL_TYPE_DAMAGE   = 1   # straight-up offensive (Basic, Light, Stinger, …)
PETAL_TYPE_TANK     = 2   # high-HP defensive (Heavy, Rock, Cactus, Bone, …)
PETAL_TYPE_HEAL     = 3   # heals the wielder (Leaf, Rose, Dahlia, Yucca, …)
PETAL_TYPE_POISON   = 4   # poison damage-over-time (Iris, Pincer, …)
PETAL_TYPE_UTILITY  = 5   # everything else: spawns, slows, anti-heal, etc.
_NUM_PETAL_TYPES_INC_NONE = 6   # number of categories, used to normalize

# PetalID -> role. Order matches PETAL_DATA / PetalID enum in
# Shared/StaticData.cc + Shared/StaticDefinitions.hh. Heavy variants are
# tagged TANK because their large health is the dominant trait in play
# (they soak more than they hit). Stinger is DAMAGE despite low health
# because that's its primary purpose. Edge-case petals (Salt, Square,
# Lotus, Cutter, …) land in UTILITY.
PETAL_TYPE = [
    PETAL_TYPE_NONE,     #  0  kNone
    PETAL_TYPE_DAMAGE,   #  1  kBasic
    PETAL_TYPE_DAMAGE,   #  2  kUnusualBasic
    PETAL_TYPE_DAMAGE,   #  3  kRareBasic
    PETAL_TYPE_DAMAGE,   #  4  kEpicBasic
    PETAL_TYPE_DAMAGE,   #  5  kLight
    PETAL_TYPE_DAMAGE,   #  6  kUnusualLight
    PETAL_TYPE_DAMAGE,   #  7  kRareLight
    PETAL_TYPE_DAMAGE,   #  8  kEpicLight
    PETAL_TYPE_TANK,     #  9  kHeavy
    PETAL_TYPE_TANK,     # 10  kUnusualHeavy
    PETAL_TYPE_TANK,     # 11  kRareHeavy
    PETAL_TYPE_TANK,     # 12  kEpicHeavy
    PETAL_TYPE_TANK,     # 13  kLegendaryHeavy
    PETAL_TYPE_DAMAGE,   # 14  kCommonStinger
    PETAL_TYPE_DAMAGE,   # 15  kStinger
    PETAL_TYPE_DAMAGE,   # 16  kRareStinger
    PETAL_TYPE_DAMAGE,   # 17  kEpicStinger
    PETAL_TYPE_HEAL,     # 18  kCommonLeaf
    PETAL_TYPE_HEAL,     # 19  kLeaf
    PETAL_TYPE_HEAL,     # 20  kRareLeaf
    PETAL_TYPE_HEAL,     # 21  kEpicLeaf
    PETAL_TYPE_HEAL,     # 22  kLegendaryLeaf
    PETAL_TYPE_DAMAGE,   # 23  kTwin
    PETAL_TYPE_HEAL,     # 24  kCommonRose
    PETAL_TYPE_HEAL,     # 25  kRose
    PETAL_TYPE_HEAL,     # 26  kRareRose
    PETAL_TYPE_HEAL,     # 27  kLegendaryRose
    PETAL_TYPE_POISON,   # 28  kCommonIris
    PETAL_TYPE_POISON,   # 29  kIris
    PETAL_TYPE_POISON,   # 30  kRareIris
    PETAL_TYPE_POISON,   # 31  kLegendaryIris
    PETAL_TYPE_UTILITY,  # 32  kMissile
    PETAL_TYPE_UTILITY,  # 33  kDandelion
    PETAL_TYPE_UTILITY,  # 34  kCommonBubble
    PETAL_TYPE_UTILITY,  # 35  kUnusualBubble
    PETAL_TYPE_UTILITY,  # 36  kBubble
    PETAL_TYPE_UTILITY,  # 37  kEpicBubble
    PETAL_TYPE_UTILITY,  # 38  kLegendaryBubble
    PETAL_TYPE_UTILITY,  # 39  kFaster
    PETAL_TYPE_TANK,     # 40  kCommonRock
    PETAL_TYPE_TANK,     # 41  kUnusualRock
    PETAL_TYPE_TANK,     # 42  kRock
    PETAL_TYPE_TANK,     # 43  kEpicRock
    PETAL_TYPE_TANK,     # 44  kLegendaryRock
    PETAL_TYPE_TANK,     # 45  kCactus
    PETAL_TYPE_UTILITY,  # 46  kWeb
    PETAL_TYPE_UTILITY,  # 47  kWing
    PETAL_TYPE_DAMAGE,   # 48  kPeas
    PETAL_TYPE_DAMAGE,   # 49  kSand
    PETAL_TYPE_POISON,   # 50  kPincer
    PETAL_TYPE_HEAL,     # 51  kDahlia
    PETAL_TYPE_DAMAGE,   # 52  kTriplet
    PETAL_TYPE_UTILITY,  # 53  kAntEgg
    PETAL_TYPE_POISON,   # 54  kBlueIris
    PETAL_TYPE_UTILITY,  # 55  kPollen
    PETAL_TYPE_POISON,   # 56  kPoisonPeas
    PETAL_TYPE_UTILITY,  # 57  kBeetleEgg
    PETAL_TYPE_HEAL,     # 58  kAzalea
    PETAL_TYPE_UTILITY,  # 59  kStick
    PETAL_TYPE_DAMAGE,   # 60  kTringer
    PETAL_TYPE_DAMAGE,   # 61  kMythicTringer
    PETAL_TYPE_UTILITY,  # 62  kTriweb
    PETAL_TYPE_UTILITY,  # 63  kAntennae
    PETAL_TYPE_TANK,     # 64  kTricac
    PETAL_TYPE_TANK,     # 65  kHeaviest
    PETAL_TYPE_UTILITY,  # 66  kThirdEye
    PETAL_TYPE_UTILITY,  # 67  kObserver
    PETAL_TYPE_POISON,   # 68  kPoisonCactus
    PETAL_TYPE_UTILITY,  # 69  kSalt
    PETAL_TYPE_DAMAGE,   # 70  kUniqueBasic
    PETAL_TYPE_UTILITY,  # 71  kSquare
    PETAL_TYPE_TANK,     # 72  kMoon
    PETAL_TYPE_UTILITY,  # 73  kLotus
    PETAL_TYPE_UTILITY,  # 74  kCutter
    PETAL_TYPE_UTILITY,  # 75  kYinYang
    PETAL_TYPE_UTILITY,  # 76  kYggdrasil
    PETAL_TYPE_DAMAGE,   # 77  kRice
    PETAL_TYPE_TANK,     # 78  kBone
    PETAL_TYPE_HEAL,     # 79  kYucca
    PETAL_TYPE_TANK,     # 80  kCorn
    # ---- wave-system rarity expansion (mirrors PETAL_RARITY indices) ----
    PETAL_TYPE_DAMAGE,   # 81  kLegendaryBasic
    PETAL_TYPE_DAMAGE,   # 82  kMythicBasic
    PETAL_TYPE_DAMAGE,   # 83  kLegendaryLight
    PETAL_TYPE_DAMAGE,   # 84  kMythicLight
    PETAL_TYPE_DAMAGE,   # 85  kUniqueLight
    PETAL_TYPE_TANK,     # 86  kMythicHeavy
    PETAL_TYPE_TANK,     # 87  kUniqueHeavy
    PETAL_TYPE_DAMAGE,   # 88  kUniqueStinger
    PETAL_TYPE_HEAL,     # 89  kMythicLeaf
    PETAL_TYPE_HEAL,     # 90  kUniqueLeaf
    PETAL_TYPE_DAMAGE,   # 91  kCommonTwin
    PETAL_TYPE_DAMAGE,   # 92  kRareTwin
    PETAL_TYPE_DAMAGE,   # 93  kEpicTwin
    PETAL_TYPE_DAMAGE,   # 94  kLegendaryTwin
    PETAL_TYPE_DAMAGE,   # 95  kMythicTwin
    PETAL_TYPE_DAMAGE,   # 96  kUniqueTwin
    PETAL_TYPE_HEAL,     # 97  kMythicRose
    PETAL_TYPE_HEAL,     # 98  kUniqueRose
    PETAL_TYPE_POISON,   # 99  kMythicIris
    PETAL_TYPE_POISON,   # 100 kUniqueIris
    PETAL_TYPE_UTILITY,  # 101 kMythicBubble
    PETAL_TYPE_UTILITY,  # 102 kUniqueBubble
    PETAL_TYPE_TANK,     # 103 kMythicRock
    PETAL_TYPE_TANK,     # 104 kUniqueRock
    PETAL_TYPE_UTILITY,  # 105 kCommonWeb
    PETAL_TYPE_UTILITY,  # 106 kUnusualWeb
    PETAL_TYPE_UTILITY,  # 107 kEpicWeb
    PETAL_TYPE_UTILITY,  # 108 kMythicWeb
    PETAL_TYPE_UTILITY,  # 109 kUniqueWeb
    PETAL_TYPE_UTILITY,  # 110 kCommonWing
    PETAL_TYPE_UTILITY,  # 111 kUnusualWing
    PETAL_TYPE_UTILITY,  # 112 kEpicWing
    PETAL_TYPE_UTILITY,  # 113 kLegendaryWing
    PETAL_TYPE_UTILITY,  # 114 kMythicWing
    PETAL_TYPE_UTILITY,  # 115 kUniqueWing
    PETAL_TYPE_DAMAGE,   # 116 kCommonPeas
    PETAL_TYPE_DAMAGE,   # 117 kUnusualPeas
    PETAL_TYPE_DAMAGE,   # 118 kLegendaryPeas
    PETAL_TYPE_DAMAGE,   # 119 kMythicPeas
    PETAL_TYPE_DAMAGE,   # 120 kUniquePeas
    PETAL_TYPE_DAMAGE,   # 121 kCommonSand
    PETAL_TYPE_DAMAGE,   # 122 kUnusualSand
    PETAL_TYPE_DAMAGE,   # 123 kEpicSand
    PETAL_TYPE_DAMAGE,   # 124 kLegendarySand
    PETAL_TYPE_DAMAGE,   # 125 kMythicSand
    PETAL_TYPE_DAMAGE,   # 126 kUniqueSand
    PETAL_TYPE_POISON,   # 127 kCommonPincer
    PETAL_TYPE_POISON,   # 128 kUnusualPincer
    PETAL_TYPE_POISON,   # 129 kEpicPincer
    PETAL_TYPE_POISON,   # 130 kLegendaryPincer
    PETAL_TYPE_POISON,   # 131 kMythicPincer
    PETAL_TYPE_POISON,   # 132 kUniquePincer
    PETAL_TYPE_HEAL,     # 133 kCommonDahlia
    PETAL_TYPE_HEAL,     # 134 kUnusualDahlia
    PETAL_TYPE_HEAL,     # 135 kEpicDahlia
    PETAL_TYPE_HEAL,     # 136 kLegendaryDahlia
    PETAL_TYPE_HEAL,     # 137 kMythicDahlia
    PETAL_TYPE_HEAL,     # 138 kUniqueDahlia
    PETAL_TYPE_DAMAGE,   # 139 kCommonTriplet
    PETAL_TYPE_DAMAGE,   # 140 kUnusualTriplet
    PETAL_TYPE_DAMAGE,   # 141 kRareTriplet
    PETAL_TYPE_DAMAGE,   # 142 kLegendaryTriplet
    PETAL_TYPE_DAMAGE,   # 143 kMythicTriplet
    PETAL_TYPE_DAMAGE,   # 144 kUniqueTriplet
    PETAL_TYPE_UTILITY,  # 145 kCommonSalt
    PETAL_TYPE_UTILITY,  # 146 kUnusualSalt
    PETAL_TYPE_UTILITY,  # 147 kEpicSalt
    PETAL_TYPE_UTILITY,  # 148 kLegendarySalt
    PETAL_TYPE_UTILITY,  # 149 kMythicSalt
    PETAL_TYPE_UTILITY,  # 150 kUniqueSalt
    PETAL_TYPE_UTILITY,  # 151 kCommonPollen
    PETAL_TYPE_UTILITY,  # 152 kUnusualPollen
    PETAL_TYPE_UTILITY,  # 153 kRarePollen
    PETAL_TYPE_UTILITY,  # 154 kLegendaryPollen
    PETAL_TYPE_UTILITY,  # 155 kMythicPollen
    PETAL_TYPE_UTILITY,  # 156 kUniquePollen
    PETAL_TYPE_UTILITY,  # 157 kCommonFaster
    PETAL_TYPE_UTILITY,  # 158 kUnusualFaster
    PETAL_TYPE_UTILITY,  # 159 kEpicFaster
    PETAL_TYPE_UTILITY,  # 160 kLegendaryFaster
    PETAL_TYPE_UTILITY,  # 161 kMythicFaster
    PETAL_TYPE_UTILITY,  # 162 kUniqueFaster
    PETAL_TYPE_TANK,     # 163 kCommonCactus
    PETAL_TYPE_TANK,     # 164 kUnusualCactus
    PETAL_TYPE_TANK,     # 165 kMythicCactus
    PETAL_TYPE_TANK,     # 166 kUniqueCactus
    PETAL_TYPE_UTILITY,  # 167 kCommonMissile
    PETAL_TYPE_UTILITY,  # 168 kUnusualMissile
    PETAL_TYPE_UTILITY,  # 169 kEpicMissile
    PETAL_TYPE_UTILITY,  # 170 kLegendaryMissile
    PETAL_TYPE_UTILITY,  # 171 kMythicMissile
    PETAL_TYPE_UTILITY,  # 172 kUniqueMissile
    PETAL_TYPE_UTILITY,  # 173 kCommonDandelion
    PETAL_TYPE_UTILITY,  # 174 kUnusualDandelion
    PETAL_TYPE_UTILITY,  # 175 kEpicDandelion
    PETAL_TYPE_UTILITY,  # 176 kLegendaryDandelion
    PETAL_TYPE_UTILITY,  # 177 kMythicDandelion
    PETAL_TYPE_UTILITY,  # 178 kUniqueDandelion
    PETAL_TYPE_HEAL,     # 179 kCommonYucca
    PETAL_TYPE_HEAL,     # 180 kRareYucca
    PETAL_TYPE_HEAL,     # 181 kEpicYucca
    PETAL_TYPE_HEAL,     # 182 kLegendaryYucca
    PETAL_TYPE_HEAL,     # 183 kMythicYucca
    PETAL_TYPE_HEAL,     # 184 kUniqueYucca
    PETAL_TYPE_TANK,     # 185 kCommonBone
    PETAL_TYPE_TANK,     # 186 kUnusualBone
    PETAL_TYPE_TANK,     # 187 kRareBone
    PETAL_TYPE_TANK,     # 188 kEpicBone
    PETAL_TYPE_TANK,     # 189 kMythicBone
    PETAL_TYPE_TANK,     # 190 kUniqueBone
    PETAL_TYPE_DAMAGE,   # 191 kCommonRice
    PETAL_TYPE_DAMAGE,   # 192 kUnusualRice
    PETAL_TYPE_DAMAGE,   # 193 kRareRice
    PETAL_TYPE_DAMAGE,   # 194 kLegendaryRice
    PETAL_TYPE_DAMAGE,   # 195 kMythicRice
    PETAL_TYPE_DAMAGE,   # 196 kUniqueRice
    PETAL_TYPE_TANK,     # 197 kCommonCorn
    PETAL_TYPE_TANK,     # 198 kUnusualCorn
    PETAL_TYPE_TANK,     # 199 kRareCorn
    PETAL_TYPE_TANK,     # 200 kLegendaryCorn
    PETAL_TYPE_TANK,     # 201 kMythicCorn
    PETAL_TYPE_TANK,     # 202 kUniqueCorn
    # ---- post-expansion appends ----
    PETAL_TYPE_DAMAGE,   # 203 kEpicPeas
    PETAL_TYPE_UTILITY,  # 204 kCommonRoot     (defend_only, stacking armor)
    PETAL_TYPE_UTILITY,  # 205 kUnusualRoot
    PETAL_TYPE_UTILITY,  # 206 kRoot
    PETAL_TYPE_UTILITY,  # 207 kEpicRoot
    PETAL_TYPE_UTILITY,  # 208 kLegendaryRoot
    PETAL_TYPE_UTILITY,  # 209 kMythicRoot
    PETAL_TYPE_UTILITY,  # 210 kUniqueRoot
]


# Per-petal effective burst (damage × count). Captures both raw damage
# and clump multiplication in a single scalar so the model can compare
# (e.g.) `kEpicLight = 35` against `kEpicHeavy = 70` and pick the harder-
# hitting one. Generated from PETAL_DATA in Shared/StaticData.cc — keep in
# lock-step with PETAL_RARITY / PETAL_TYPE.
_PETAL_MAX_BURST = 150  # Mythic Tringer: 50 damage × 3 count
PETAL_BURST = [
    0,    #  0  kNone
    10,   #  1  kBasic
    16,   #  2  kUnusualBasic
    25,   #  3  kRareBasic
    40,   #  4  kEpicBasic
    8,    #  5  kLight
    14,   #  6  kUnusualLight
    22,   #  7  kRareLight
    35,   #  8  kEpicLight
    20,   #  9  kHeavy
    30,   # 10  kUnusualHeavy
    45,   # 11  kRareHeavy
    70,   # 12  kEpicHeavy
    100,  # 13  kLegendaryHeavy
    20,   # 14  kCommonStinger
    35,   # 15  kStinger
    50,   # 16  kRareStinger
    75,   # 17  kEpicStinger
    6,    # 18  kCommonLeaf
    8,    # 19  kLeaf
    12,   # 20  kRareLeaf
    18,   # 21  kEpicLeaf
    25,   # 22  kLegendaryLeaf
    16,   # 23  kTwin (8 × 2)
    3,    # 24  kCommonRose
    5,    # 25  kRose
    7,    # 26  kRareRose
    10,   # 27  kLegendaryRose
    3,    # 28  kCommonIris
    5,    # 29  kIris
    7,    # 30  kRareIris
    15,   # 31  kLegendaryIris
    25,   # 32  kMissile
    10,   # 33  kDandelion
    0,    # 34  kCommonBubble
    0,    # 35  kUnusualBubble
    0,    # 36  kBubble
    0,    # 37  kEpicBubble
    0,    # 38  kLegendaryBubble
    7,    # 39  kFaster
    5,    # 40  kCommonRock
    7,    # 41  kUnusualRock
    10,   # 42  kRock
    15,   # 43  kEpicRock
    25,   # 44  kLegendaryRock
    5,    # 45  kCactus
    5,    # 46  kWeb
    15,   # 47  kWing
    32,   # 48  kPeas (8 × 4)
    12,   # 49  kSand (3 × 4)
    5,    # 50  kPincer
    15,   # 51  kDahlia (5 × 3)
    24,   # 52  kTriplet (8 × 3)
    2,    # 53  kAntEgg (1 × 2)
    5,    # 54  kBlueIris
    24,   # 55  kPollen (8 × 3)
    8,    # 56  kPoisonPeas (2 × 4)
    1,    # 57  kBeetleEgg
    5,    # 58  kAzalea
    1,    # 59  kStick
    105,  # 60  kTringer (35 × 3)
    150,  # 61  kMythicTringer (50 × 3)
    15,   # 62  kTriweb (5 × 3)
    0,    # 63  kAntennae
    15,   # 64  kTricac (5 × 3)
    10,   # 65  kHeaviest
    0,    # 66  kThirdEye
    0,    # 67  kObserver
    5,    # 68  kPoisonCactus
    10,   # 69  kSalt
    10,   # 70  kUniqueBasic
    10,   # 71  kSquare
    1,    # 72  kMoon
    5,    # 73  kLotus
    0,    # 74  kCutter
    15,   # 75  kYinYang
    1,    # 76  kYggdrasil
    4,    # 77  kRice
    10,   # 78  kBone
    5,    # 79  kYucca
    2,    # 80  kCorn
    # ---- wave-system rarity expansion (formula = round(damage × count),
    # damage = base_damage × 1.5^Δ_rarity from the existing tier; values
    # exceeding _PETAL_MAX_BURST are clamped to 1.0 by petal_burst_norm) ----
    60,   # 81  kLegendaryBasic   (40 × 1.5)
    90,   # 82  kMythicBasic      (40 × 2.25)
    52,   # 83  kLegendaryLight   (35 × 1.5)
    79,   # 84  kMythicLight      (35 × 2.25)
    118,  # 85  kUniqueLight      (35 × 3.375)
    150,  # 86  kMythicHeavy      (100 × 1.5)
    225,  # 87  kUniqueHeavy      (100 × 2.25)
    225,  # 88  kUniqueStinger    (50 × 1.5 × 3 count)
    38,   # 89  kMythicLeaf       (25 × 1.5)
    56,   # 90  kUniqueLeaf       (25 × 2.25)
    10,   # 91  kCommonTwin       (5 × 2)
    24,   # 92  kRareTwin         (12 × 2)
    36,   # 93  kEpicTwin         (18 × 2)
    54,   # 94  kLegendaryTwin    (27 × 2)
    80,   # 95  kMythicTwin       (40 × 2)
    122,  # 96  kUniqueTwin       (61 × 2)
    15,   # 97  kMythicRose       (10 × 1.5)
    22,   # 98  kUniqueRose       (10 × 2.25)
    22,   # 99  kMythicIris       (15 × 1.5)
    34,   # 100 kUniqueIris       (15 × 2.25)
    0,    # 101 kMythicBubble
    0,    # 102 kUniqueBubble
    38,   # 103 kMythicRock       (25 × 1.5)
    56,   # 104 kUniqueRock       (25 × 2.25)
    2,    # 105 kCommonWeb
    3,    # 106 kUnusualWeb
    7,    # 107 kEpicWeb
    51,   # 108 kMythicWeb        (17 × 3 count)
    75,   # 109 kUniqueWeb        (25 × 3 count)
    7,    # 110 kCommonWing
    10,   # 111 kUnusualWing
    22,   # 112 kEpicWing
    34,   # 113 kLegendaryWing
    51,   # 114 kMythicWing
    76,   # 115 kUniqueWing
    16,   # 116 kCommonPeas       (4 × 4)
    20,   # 117 kUnusualPeas      (5 × 4)
    72,   # 118 kLegendaryPeas    (18 × 4)
    108,  # 119 kMythicPeas       (27 × 4)
    160,  # 120 kUniquePeas       (40 × 4)
    4,    # 121 kCommonSand       (1 × 4)
    8,    # 122 kUnusualSand      (2 × 4)
    16,   # 123 kEpicSand         (4 × 4)
    28,   # 124 kLegendarySand    (7 × 4)
    40,   # 125 kMythicSand       (10 × 4)
    60,   # 126 kUniqueSand       (15 × 4)
    2,    # 127 kCommonPincer
    3,    # 128 kUnusualPincer
    7,    # 129 kEpicPincer
    11,   # 130 kLegendaryPincer
    17,   # 131 kMythicPincer
    25,   # 132 kUniquePincer
    6,    # 133 kCommonDahlia     (2 × 3)
    9,    # 134 kUnusualDahlia    (3 × 3)
    21,   # 135 kEpicDahlia       (7 × 3)
    33,   # 136 kLegendaryDahlia  (11 × 3)
    51,   # 137 kMythicDahlia     (17 × 3)
    75,   # 138 kUniqueDahlia     (25 × 3)
    6,    # 139 kCommonTriplet    (2 × 3)
    12,   # 140 kUnusualTriplet   (4 × 3)
    15,   # 141 kRareTriplet      (5 × 3)
    36,   # 142 kLegendaryTriplet (12 × 3)
    54,   # 143 kMythicTriplet    (18 × 3)
    81,   # 144 kUniqueTriplet    (27 × 3)
    4,    # 145 kCommonSalt
    7,    # 146 kUnusualSalt
    15,   # 147 kEpicSalt
    22,   # 148 kLegendarySalt
    34,   # 149 kMythicSalt
    51,   # 150 kUniqueSalt
    6,    # 151 kCommonPollen     (2 × 3)
    12,   # 152 kUnusualPollen    (4 × 3)
    15,   # 153 kRarePollen       (5 × 3)
    36,   # 154 kLegendaryPollen  (12 × 3)
    54,   # 155 kMythicPollen     (18 × 3)
    81,   # 156 kUniquePollen     (27 × 3)
    3,    # 157 kCommonFaster
    5,    # 158 kUnusualFaster
    10,   # 159 kEpicFaster
    16,   # 160 kLegendaryFaster
    24,   # 161 kMythicFaster
    35,   # 162 kUniqueFaster
    2,    # 163 kCommonCactus
    3,    # 164 kUnusualCactus
    17,   # 165 kMythicCactus
    25,   # 166 kUniqueCactus
    11,   # 167 kCommonMissile
    17,   # 168 kUnusualMissile
    38,   # 169 kEpicMissile
    56,   # 170 kLegendaryMissile
    84,   # 171 kMythicMissile
    127,  # 172 kUniqueMissile
    4,    # 173 kCommonDandelion
    7,    # 174 kUnusualDandelion
    15,   # 175 kEpicDandelion
    22,   # 176 kLegendaryDandelion
    34,   # 177 kMythicDandelion
    51,   # 178 kUniqueDandelion
    3,    # 179 kCommonYucca
    7,    # 180 kRareYucca
    11,   # 181 kEpicYucca
    17,   # 182 kLegendaryYucca
    25,   # 183 kMythicYucca
    38,   # 184 kUniqueYucca
    2,    # 185 kCommonBone
    3,    # 186 kUnusualBone
    4,    # 187 kRareBone
    7,    # 188 kEpicBone
    15,   # 189 kMythicBone
    22,   # 190 kUniqueBone
    1,    # 191 kCommonRice
    2,    # 192 kUnusualRice
    3,    # 193 kRareRice
    6,    # 194 kLegendaryRice
    9,    # 195 kMythicRice
    13,   # 196 kUniqueRice
    1,    # 197 kCommonCorn
    1,    # 198 kUnusualCorn
    2,    # 199 kRareCorn
    4,    # 200 kLegendaryCorn
    6,    # 201 kMythicCorn
    8,    # 202 kUniqueCorn
    # ---- post-expansion appends. Root has count=1 and damage = 10 ×
    # 1.5^Δ from kEpicRoot (rank 3); kEpicPeas is 12 dmg × 4 count = 48. ----
    48,   # 203 kEpicPeas        (12 × 4)
    3,    # 204 kCommonRoot      (10 / 3.375)
    4,    # 205 kUnusualRoot     (10 / 2.25)
    7,    # 206 kRoot            (10 / 1.5)
    10,   # 207 kEpicRoot
    15,   # 208 kLegendaryRoot
    22,   # 209 kMythicRoot      (10 × 2.25)
    33,   # 210 kUniqueRoot      (10 × 3.375)
]


def petal_burst_norm(petal_id: int) -> float:
    """Effective petal burst (damage × count) normalised to [0, 1]. Empty
    or out-of-range maps to 0. New rarity-expansion petals can exceed
    _PETAL_MAX_BURST (kept at the original 150 so already-trained
    checkpoints don't see their input distribution shift); we clamp here
    so the network always sees a value in [0, 1]."""
    if 0 <= petal_id < len(PETAL_BURST):
        v = PETAL_BURST[petal_id] / _PETAL_MAX_BURST
        return v if v < 1.0 else 1.0
    return 0.0


def petal_type(petal_id: int) -> int:
    """Return the PETAL_TYPE_* role for a petal id, or PETAL_TYPE_NONE for
    out-of-range / sentinel ids."""
    if 0 <= petal_id < len(PETAL_TYPE):
        return PETAL_TYPE[petal_id]
    return PETAL_TYPE_NONE


def petal_type_norm(petal_id: int) -> float:
    """petal_type() normalised to [0, 1] so it can drop straight into a
    state-vector slot. Empty (kNone) maps to 0.0; UTILITY maps to 1.0."""
    t = petal_type(petal_id)
    return t / (_NUM_PETAL_TYPES_INC_NONE - 1)


def petal_rank(petal_id: int) -> int:
    """Return a rank where higher = better. kNone is worst."""
    if petal_id == PETAL_NONE:
        return -1
    if 0 <= petal_id < len(PETAL_RARITY):
        return PETAL_RARITY[petal_id]
    return -1

# Clientbound
C_DISCONNECT = 0
C_CLIENT_UPDATE = 1
C_OUTDATED = 2
C_CHAT = 3            # { u8 op, string sender_name, u8 sender_color, string text }
# { u8 op, string winner_name, u32 winner_score } — broadcast by the server
# at WAVE_TICKS_PER_ROUND boundaries. By the time we receive this, every
# flower in the simulation has been killed and every camera's inventory
# wiped, so a respawn after this packet is treated as a "new round" start.
C_ROUND_END = 4

# Input flags (Shared/StaticDefinitions.hh InputFlags)
INPUT_ATTACK = 1 << 0
INPUT_DEFEND = 1 << 1

# Mirrors Shared/Config.cc:MAX_CHAT_LENGTH. Server enforces this; we truncate
# locally so packets we send aren't dropped.
MAX_CHAT_LENGTH = 80


class Writer:
    def __init__(self) -> None:
        self.buf = bytearray()

    def w_u8(self, v: int) -> None:
        self.buf.append(v & 0xFF)

    def _w_uvarint(self, v: int) -> None:
        v &= 0xFFFFFFFFFFFFFFFF
        while v > 0x7F:
            self.buf.append((v & 0x7F) | 0x80)
            v >>= 7
        self.buf.append(v)

    def w_u16(self, v: int) -> None:
        self._w_uvarint(v)

    def w_u32(self, v: int) -> None:
        self._w_uvarint(v)

    def w_u64(self, v: int) -> None:
        self._w_uvarint(v)

    def _w_ivarint(self, v: int, width_mask: int) -> None:
        sign = 1 if v < 0 else 0
        if sign:
            v = -v
        self._w_uvarint(((v << 1) | sign) & width_mask)

    def w_i32(self, v: int) -> None:
        self._w_ivarint(v, 0xFFFFFFFF)

    def w_i64(self, v: int) -> None:
        self._w_ivarint(v, 0xFFFFFFFFFFFFFFFF)

    def w_float(self, v: float) -> None:
        self.w_i64(int(v * PROTOCOL_FLOAT_SCALE))

    def w_string(self, s: str) -> None:
        encoded = s.encode("utf-8")
        self.w_u32(len(encoded))
        self.buf.extend(encoded)

    def w_eid(self, eid: tuple[int, int]) -> None:
        i, h = eid
        self.w_u16(i)
        if i:
            self.w_u8(h)

    def to_bytes(self) -> bytes:
        return bytes(self.buf)


class Reader:
    def __init__(self, data: bytes | bytearray | memoryview) -> None:
        self.data = bytes(data)
        self.pos = 0

    def remaining(self) -> int:
        return len(self.data) - self.pos

    def r_u8(self) -> int:
        v = self.data[self.pos]
        self.pos += 1
        return v

    def _r_uvarint(self, max_bytes: int) -> int:
        ret = 0
        for i in range(max_bytes):
            o = self.r_u8()
            ret |= (o & 0x7F) << (i * 7)
            if o <= 0x7F:
                break
        return ret

    def r_u16(self) -> int:
        return self._r_uvarint(5)

    def r_u32(self) -> int:
        return self._r_uvarint(5)

    def r_u64(self) -> int:
        return self._r_uvarint(10)

    def r_i32(self) -> int:
        r = self.r_u32()
        s = r & 1
        v = r >> 1
        return -v if s else v

    def r_i64(self) -> int:
        r = self.r_u64()
        s = r & 1
        v = r >> 1
        return -v if s else v

    def r_float(self) -> float:
        return self.r_i64() / PROTOCOL_FLOAT_SCALE

    def r_string(self) -> str:
        n = self.r_u32()
        s = self.data[self.pos:self.pos + n].decode("utf-8", errors="replace")
        self.pos += n
        return s

    def r_eid(self) -> tuple[int, int]:
        i = self.r_u16()
        h = self.r_u8() if i else 0
        return (i, h)


# ---------------------------------------------------------------------------
# Entity layout. Order MUST match Shared/EntityDef.hh PERCOMPONENT / PERFIELD.
# ---------------------------------------------------------------------------

COMPONENTS = [
    "Physics", "Camera", "Relations", "Flower", "Petal",
    "Health", "Mob", "Drop", "Segmented", "Web", "Score", "Name",
]
COMPONENT_BIT = {name: i for i, name in enumerate(COMPONENTS)}

# (component, field_name, type, multi_count) where multi_count==0 means SINGLE
FIELDS: list[tuple[str, str, str, int]] = [
    ("Physics",   "x",                "float",  0),
    ("Physics",   "y",                "float",  0),
    ("Physics",   "radius",           "float",  0),
    ("Physics",   "angle",            "float",  0),
    ("Camera",    "player",           "eid",    0),
    ("Camera",    "respawn_level",    "u8",     0),
    ("Camera",    "inventory",        "u8",    16),
    ("Camera",    "killed_by",        "string", 0),
    ("Camera",    "camera_x",         "float",  0),
    ("Camera",    "camera_y",         "float",  0),
    ("Camera",    "fov",              "float",  0),
    ("Relations", "team",             "eid",    0),
    ("Relations", "parent",           "eid",    0),
    ("Relations", "color",            "u8",     0),
    ("Flower",    "eye_angle",        "float",  0),
    ("Flower",    "overlevel_timer",  "float",  0),
    ("Flower",    "loadout_count",    "u8",     0),
    ("Flower",    "face_flags",       "u8",     0),
    ("Flower",    "loadout_ids",      "u8",    16),
    ("Flower",    "loadout_reloads",  "u8",     8),
    ("Petal",     "petal_id",         "u8",     0),
    ("Health",    "health_ratio",     "float",  0),
    ("Health",    "damaged",          "u8",     0),
    ("Mob",       "mob_id",           "u8",     0),
    ("Mob",       "mob_rarity",       "u8",     0),
    ("Drop",      "drop_id",          "u8",     0),
    ("Segmented", "is_tail",          "u8",     0),
    ("Score",     "score",            "u32",    0),
    ("Name",      "name",             "string", 0),
    ("Name",      "nametag_visible",  "u8",     0),
]
FIELD_COUNT = len(FIELDS)

# fields per component, preserving declaration order
COMPONENT_FIELDS: dict[str, list[tuple[int, str, str, int]]] = {c: [] for c in COMPONENTS}
for idx, (comp, name, ty, mc) in enumerate(FIELDS):
    COMPONENT_FIELDS[comp].append((idx, name, ty, mc))


def _read_one(reader: Reader, ty: str):
    if ty == "u8":     return reader.r_u8()
    if ty == "u32":    return reader.r_u32()
    if ty == "i32":    return reader.r_i32()
    if ty == "i64":    return reader.r_i64()
    if ty == "float":  return reader.r_float()
    if ty == "eid":    return reader.r_eid()
    if ty == "string": return reader.r_string()
    raise ValueError(f"unknown field type {ty}")


def has_component(ent: dict, comp_name: str) -> bool:
    bits = ent.get("_components", 0)
    return ((bits >> COMPONENT_BIT[comp_name]) & 1) == 1


def _read_entity_create(reader: Reader, entity: dict) -> None:
    components = reader.r_u32()
    lifetime = reader.r_u32()
    entity["_components"] = components
    entity["_lifetime"] = lifetime
    for i, comp in enumerate(COMPONENTS):
        if not (components >> i) & 1:
            continue
        for _idx, name, ty, mc in COMPONENT_FIELDS[comp]:
            if mc > 0:
                arr = entity.get(name)
                if arr is None or len(arr) != mc:
                    arr = [0] * mc
                    entity[name] = arr
                for j in range(mc):
                    arr[j] = _read_one(reader, ty)
            else:
                entity[name] = _read_one(reader, ty)


def _read_entity_update(reader: Reader, entity: dict) -> None:
    while True:
        idx = reader.r_u8()
        if idx == FIELD_COUNT:
            return
        if idx > FIELD_COUNT:
            # corrupt — bail to avoid infinite loop
            return
        _comp, name, ty, mc = FIELDS[idx]
        if mc > 0:
            arr = entity.get(name)
            if arr is None or len(arr) != mc:
                arr = [0] * mc
                entity[name] = arr
            while True:
                k = reader.r_u8()
                if k >= mc:
                    break
                arr[k] = _read_one(reader, ty)
        else:
            entity[name] = _read_one(reader, ty)


# Arena / leaderboard layout. Mirrors FIELDS_Arena in Shared/Arena.hh.
# (field_idx, name, type, multi_count). multi_count = 0 for SINGLE; otherwise
# the array length (LEADERBOARD_SIZE = 10 for the leaderboard fields).
LEADERBOARD_SIZE = 10
ARENA_FIELDS: list[tuple[str, str, int]] = [
    ("player_count", "u32",    0),                  # 0
    ("scores",       "float", LEADERBOARD_SIZE),    # 1
    ("names",        "string", LEADERBOARD_SIZE),   # 2
    ("colors",       "u8",    LEADERBOARD_SIZE),    # 3
]
ARENA_FIELD_COUNT = len(ARENA_FIELDS)


def _new_arena() -> dict:
    return {
        "player_count": 0,
        "scores": [0.0] * LEADERBOARD_SIZE,
        "names": [""] * LEADERBOARD_SIZE,
        "colors": [0] * LEADERBOARD_SIZE,
    }


def _read_arena_full(reader: Reader, arena: dict) -> None:
    for name, ty, mc in ARENA_FIELDS:
        if mc > 0:
            arr = arena.setdefault(name, [None] * mc)
            for j in range(mc):
                arr[j] = _read_one(reader, ty)
        else:
            arena[name] = _read_one(reader, ty)


def _read_arena_sparse(reader: Reader, arena: dict) -> None:
    while True:
        idx = reader.r_u8()
        if idx == ARENA_FIELD_COUNT:
            return
        if idx > ARENA_FIELD_COUNT:
            return  # corrupt — bail
        name, ty, mc = ARENA_FIELDS[idx]
        if mc > 0:
            arr = arena.setdefault(name, [None] * mc)
            while True:
                k = reader.r_u8()
                if k >= mc:
                    break
                arr[k] = _read_one(reader, ty)
        else:
            arena[name] = _read_one(reader, ty)


def parse_client_update(
    data: bytes,
    entities: dict,
    arena: dict | None = None,
) -> tuple[int, int] | None:
    """Mutates `entities` (eid -> dict) and `arena` (optional dict mirroring
    Shared/Arena.hh: player_count, scores[10], names[10], colors[10]). Returns
    the camera EntityID, or None on parse error.

    Pass the same `arena` dict each call so sparse updates land on top of
    accumulated state. If `arena` is None, the trailer is consumed but
    discarded (keeps the parser draining the packet without forcing callers
    that don't care about the leaderboard to allocate one).
    """
    r = Reader(data)
    if r.r_u8() != C_CLIENT_UPDATE:
        return None
    camera_id = r.r_eid()

    # deletes — terminated by NULL_ENTITY (id == 0)
    while True:
        eid = r.r_eid()
        if eid[0] == 0:
            break
        entities.pop(eid, None)

    # creates / updates — terminated by NULL_ENTITY
    while True:
        eid = r.r_eid()
        if eid[0] == 0:
            break
        flags = r.r_u8()
        create = flags & 1
        pending_delete = (flags >> 1) & 1
        if create:
            ent = {"_id": eid, "_pending_delete": pending_delete}
            entities[eid] = ent
            _read_entity_create(r, ent)
        else:
            ent = entities.get(eid)
            if ent is None:
                # shouldn't happen, but recover by treating as create-equivalent skeleton
                ent = {"_id": eid, "_components": 0, "_pending_delete": pending_delete}
                entities[eid] = ent
            ent["_pending_delete"] = pending_delete
            _read_entity_update(r, ent)

    # Arena trailer. The flag byte mirrors the server's `seen_arena` state
    # (Server/Game.cc:_update_client). Note the inverted-looking convention:
    # the *first* update for a new client sends flag=0 with a sparse encoding
    # (delta from default-constructed Arena). Subsequent updates send flag=1
    # with a full encoding. We honour both.
    sink = arena if arena is not None else _new_arena()
    try:
        seen = r.r_u8()
        if seen:
            _read_arena_full(r, sink)
        else:
            _read_arena_sparse(r, sink)
    except IndexError:
        # Older server / truncated packet — leave arena as-is.
        pass

    return camera_id


def build_chat_packet(text: str) -> bytes:
    """Serverbound chat. Truncate to MAX_CHAT_LENGTH so the server doesn't drop us."""
    w = Writer()
    w.w_u8(S_CLIENT_CHAT)
    w.w_string(text[:MAX_CHAT_LENGTH])
    return w.to_bytes()


def parse_chat_packet(data: bytes) -> tuple[str, int, str] | None:
    """Returns (sender_name, sender_color, text) or None on parse error."""
    try:
        r = Reader(data)
        if r.r_u8() != C_CHAT:
            return None
        sender_name = r.r_string()
        sender_color = r.r_u8()
        text = r.r_string()
        return sender_name, sender_color, text
    except (IndexError, ValueError):
        return None


def parse_round_end(data: bytes) -> tuple[str, int] | None:
    """Decode a C_ROUND_END packet → (winner_name, winner_score). Returns
    None on parse error or non-roundend opcode."""
    try:
        r = Reader(data)
        if r.r_u8() != C_ROUND_END:
            return None
        winner_name = r.r_string()
        winner_score = r.r_u32()
        return winner_name, winner_score
    except (IndexError, ValueError):
        return None


# ---------------------------------------------------------------------------
# Data-chat: bot-to-bot structured messages over the chat channel.
#
# The chat field is a UTF-8 string. We piggyback on it: any line whose first
# code unit is DATA_MARKER (\x01, valid UTF-8 single byte, accepted by the
# server's UTF8Parser::is_valid_utf8 because any byte < 0x80 passes) is a
# binary message. The remainder is base64 of `[u8 kind] [kind-specific
# varint/float fields]`, encoded with the same Writer/Reader the rest of the
# protocol uses — so floats are int64-of-(value*64) varints, EntityIDs are
# (u16 id, optional u8 hash), etc.
#
# Budget: MAX_CHAT_LENGTH=80. After the marker that's 79 chars of base64,
# carrying ~58 bytes of binary payload — plenty for any of the kinds below.
# ---------------------------------------------------------------------------

import base64 as _b64

DATA_MARKER = "\x01"

MSG_POSITION = 1   # { float x, float y, float hp_ratio }
MSG_KILL     = 2   # { u32 score_delta, float x, float y }
MSG_HELP     = 3   # { float x, float y, float hp_ratio }   "I'm in trouble"
MSG_TARGET   = 4   # { eid target, float x, float y }       "I'm pursuing this"
MSG_THREAT   = 5   # { u8 mob_id, float x, float y }        "danger here"


def _data_chat(payload: bytes) -> str:
    return DATA_MARKER + _b64.b64encode(payload).decode("ascii")


def encode_position(x: float, y: float, hp: float) -> str:
    w = Writer()
    w.w_u8(MSG_POSITION); w.w_float(x); w.w_float(y); w.w_float(hp)
    return _data_chat(w.to_bytes())


def encode_kill(score_delta: int, x: float, y: float) -> str:
    w = Writer()
    w.w_u8(MSG_KILL); w.w_u32(int(score_delta)); w.w_float(x); w.w_float(y)
    return _data_chat(w.to_bytes())


def encode_help(x: float, y: float, hp: float) -> str:
    w = Writer()
    w.w_u8(MSG_HELP); w.w_float(x); w.w_float(y); w.w_float(hp)
    return _data_chat(w.to_bytes())


def encode_target(eid: tuple[int, int], x: float, y: float) -> str:
    w = Writer()
    w.w_u8(MSG_TARGET); w.w_eid(eid); w.w_float(x); w.w_float(y)
    return _data_chat(w.to_bytes())


def encode_threat(mob_id: int, x: float, y: float) -> str:
    w = Writer()
    w.w_u8(MSG_THREAT); w.w_u8(int(mob_id) & 0xFF); w.w_float(x); w.w_float(y)
    return _data_chat(w.to_bytes())


def decode_data_chat(text: str) -> dict | None:
    """Parse a structured chat line. Returns None if `text` is plain human
    text (so callers can fall through to text-handling). Returns None on a
    truncated or malformed payload too — we never raise here, since this is
    untrusted input from another bot/peer."""
    if not text or text[0] != DATA_MARKER:
        return None
    try:
        raw = _b64.b64decode(text[1:].encode("ascii"), validate=True)
    except (ValueError, _b64.binascii.Error):  # type: ignore[attr-defined]
        return None
    if not raw:
        return None
    try:
        r = Reader(raw)
        kind = r.r_u8()
        if kind == MSG_POSITION:
            return {"kind": "position", "x": r.r_float(), "y": r.r_float(), "hp": r.r_float()}
        if kind == MSG_KILL:
            return {"kind": "kill", "score": r.r_u32(), "x": r.r_float(), "y": r.r_float()}
        if kind == MSG_HELP:
            return {"kind": "help", "x": r.r_float(), "y": r.r_float(), "hp": r.r_float()}
        if kind == MSG_TARGET:
            return {"kind": "target", "eid": r.r_eid(), "x": r.r_float(), "y": r.r_float()}
        if kind == MSG_THREAT:
            return {"kind": "threat", "mob_id": r.r_u8(), "x": r.r_float(), "y": r.r_float()}
    except (IndexError, ValueError):
        return None
    return None
