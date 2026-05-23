"""Pure-Python build targets for reward shaping.

The server has no knowledge of "builds" — every player always spawns
with the original kBasic ×5 starting loadout (Server/Game.cc::add_client).
The bot earns the demonstrated build through normal gameplay: kill
mobs, pick up petal drops, swap them into primary. This module wires
the "you are now playing the right build" reward signal.

Match semantics are *rarity-agnostic*: a Common Stinger counts the
same as an Epic Stinger for matching a "Stinger" target. The model
should learn that a build is a recipe of petal *families* (Heavy,
Stinger, Rose, …) — picking up any tier of a target family makes
progress. This dovetails with the natural drop curve: a fresh
basic-spawn bot mostly sees Common drops, and we don't want it stuck
at 0% match just because it can't farm Epics yet.

PETAL_FAMILY_BY_TYPE below maps every PetalType (Shared/StaticDefinitions.hh
PetalType enum) to a short family string. Petals on the wire are now
2-byte (type, rarity) pairs and are carried in Python as packed ints
`(type << 8) | rarity` (see protocol.py:pack_petal). Rarity has been
factored out of the family axis — "family" here is just the mechanical
type — so the family table shrunk from 200+ entries to one per
PetalType (49). If a new PetalType is appended, add it here too or it
will classify as "unknown" and never count toward any build.
"""

from __future__ import annotations

from collections import Counter

from protocol import PetalType, petal_type_of

# Build identifiers. Pure-Python — the server doesn't see these.
BUILD_BASIC   = 0
BUILD_DAMAGE  = 1
BUILD_TANK    = 2
BUILD_HEAL    = 3
BUILD_POISON  = 4
BUILD_MIXED   = 5

BUILD_NAMES = {
    "basic":  BUILD_BASIC,
    "damage": BUILD_DAMAGE,
    "tank":   BUILD_TANK,
    "heal":   BUILD_HEAL,
    "poison": BUILD_POISON,
    "mixed":  BUILD_MIXED,
}
BUILD_NAME_BY_ID = {v: k for k, v in BUILD_NAMES.items()}

# PetalType → family. One entry per mechanical family in the PetalType
# enum (49 rows including kNone). Rarity doesn't affect family, so this
# table is keyed by the type byte only — the rarity byte of a packed
# petal id is irrelevant for matching against a target loadout.
#
# Split-variant naming follows the migration spec:
#   kStinger / kTringer       → "stinger" (Tringer is the clump variant)
#   kCactus / kTricac         → "cactus"  (Tricac is the legendary clump)
#   kPoisonCactus             → "cactus"  (kept with cactus family — Poison
#                                          Cactus replaced legacy ids 45/68
#                                          which were already "cactus")
#   kIris / kBlueIris         → "iris"
#   kRose / kAzalea           → kRose: "rose", kAzalea: "azalea"
#   kPeas / kPoisonPeas       → "peas"
#   kWeb / kTriweb            → "web"
#   kAntEgg / kBeetleEgg      → "egg"   (matches legacy collapsing of both)
PETAL_FAMILY_BY_TYPE: list[str] = [
    "none",        #  0  kNone
    "basic",       #  1  kBasic
    "light",       #  2  kLight
    "heavy",       #  3  kHeavy
    "stinger",     #  4  kStinger
    "stinger",     #  5  kTringer (clump-variant of stinger; same family)
    "leaf",        #  6  kLeaf
    "twin",        #  7  kTwin
    "rose",        #  8  kRose
    "azalea",      #  9  kAzalea
    "iris",        # 10  kIris
    "iris",        # 11  kBlueIris (kept with iris family for matching)
    "missile",     # 12  kMissile
    "dandelion",   # 13  kDandelion
    "bubble",      # 14  kBubble
    "faster",      # 15  kFaster
    "rock",        # 16  kRock
    "cactus",      # 17  kCactus
    "cactus",      # 18  kPoisonCactus (kept with cactus family)
    "cactus",      # 19  kTricac (Tricac is the legendary clump variant)
    "web",         # 20  kWeb
    "web",         # 21  kTriweb
    "wing",        # 22  kWing
    "peas",        # 23  kPeas
    "peas",        # 24  kPoisonPeas
    "sand",        # 25  kSand
    "pincer",      # 26  kPincer
    "dahlia",      # 27  kDahlia
    "triplet",     # 28  kTriplet
    "egg",         # 29  kAntEgg
    "egg",         # 30  kBeetleEgg
    "pollen",      # 31  kPollen
    "stick",       # 32  kStick
    "antennae",    # 33  kAntennae
    "heavy",       # 34  kHeaviest (Heaviest belongs to heavy family)
    "thirdeye",    # 35  kThirdEye
    "observer",    # 36  kObserver
    "salt",        # 37  kSalt
    "square",      # 38  kSquare
    "moon",        # 39  kMoon
    "lotus",       # 40  kLotus
    "cutter",      # 41  kCutter
    "yinyang",     # 42  kYinYang
    "yggdrasil",   # 43  kYggdrasil
    "rice",        # 44  kRice
    "bone",        # 45  kBone
    "yucca",       # 46  kYucca
    "corn",        # 47  kCorn
    "root",        # 48  kRoot
]
assert len(PETAL_FAMILY_BY_TYPE) == PetalType.kNumPetalTypes, (
    f"PETAL_FAMILY_BY_TYPE has {len(PETAL_FAMILY_BY_TYPE)} entries; "
    f"expected {PetalType.kNumPetalTypes}"
)


def family_of(petal_id: int) -> str:
    """Return the rarity-agnostic family name for a packed petal id
    (`(type << 8) | rarity`). Unknown types — e.g. a type byte that hasn't
    been registered here — return "unknown" so they won't match anything
    and won't count against the bot's progress, just silently miss out on
    whatever family they belong to."""
    t = petal_type_of(int(petal_id))
    if 0 <= t < len(PETAL_FAMILY_BY_TYPE):
        return PETAL_FAMILY_BY_TYPE[t]
    return "unknown"


# Target builds, expressed as 5-element primary-loadout family lists.
# Order doesn't matter — match_score treats both target and current as
# multisets. Storage slots aren't shaped: the model needs to learn
# storage management on its own, the build target only cares about
# what's actively orbiting.
BUILDS_TARGETS: dict[int, list[str]] = {
    BUILD_DAMAGE: ["stinger", "stinger", "stinger", "heavy", "heavy"],
    BUILD_TANK:   ["rock", "rock", "rock", "cactus", "cactus"],
    BUILD_HEAL:   ["leaf", "leaf", "rose", "rose", "leaf"],
    BUILD_POISON: ["iris", "iris", "iris", "pincer", "pincer"],
    BUILD_MIXED:  ["heavy", "stinger", "leaf", "rock", "light"],
}


def build_id_from_name(name: str) -> int:
    """Case-insensitive parse, returning BUILD_BASIC for unknown
    names so a typo on the CLI falls back to "no shaping" rather
    than crashing."""
    if not name:
        return BUILD_BASIC
    return BUILD_NAMES.get(name.lower(), BUILD_BASIC)


def primary_target_families(build_id: int) -> list[str]:
    """Family list for the build's primary loadout. Returns [] for
    BUILD_BASIC so callers can skip shaping when there's no real
    target."""
    if build_id == BUILD_BASIC:
        return []
    return list(BUILDS_TARGETS.get(build_id, []))


def match_score(current_petal_ids: list[int], target_families: list[str]) -> float:
    """Multiset overlap between (the families of) current primary petals
    and the target family list, normalised to [0, 1]. 0 = no target
    family represented; 1 = every target family slot covered (with
    multiplicity).

        target = ["stinger", "stinger", "heavy", "heavy", "light"]
        current = [kEpicStinger, kStinger, kBasic, kBasic, kEpicLight]
          → families = [stinger, stinger, basic, basic, light]
          → matches = min(2, 2) [stinger] + min(2, 0) [heavy] + min(1, 1) [light]
          → = 2 + 0 + 1 = 3 → 3/5 = 0.6

    Note: `current_petal_ids` items are the packed `(type << 8) | rarity`
    ints that come over the wire — `family_of` strips the rarity byte
    internally, so callers don't need to.
    """
    if not target_families:
        return 0.0
    tc = Counter(target_families)
    cc = Counter(family_of(p) for p in current_petal_ids)
    matched = 0
    for fam, want in tc.items():
        matched += min(want, cc.get(fam, 0))
    return matched / len(target_families)
