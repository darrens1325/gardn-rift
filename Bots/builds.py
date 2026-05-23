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

PETAL_FAMILY below maps every server-side PetalID (Shared/StaticDefinitions.hh
PetalID enum) to a short family string. Keep in lock-step with that
enum — if a new petal is appended, add it here too or it'll classify
as "unknown" and never count toward any build.
"""

from __future__ import annotations

from collections import Counter

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

# PetalID → family. Family is the rarity-stripped name from
# Shared/StaticData.cc PETAL_DATA[i].name (so "kEpicStinger",
# "kRareStinger", "kStinger", "kCommonStinger" all collapse to
# "stinger"). Built by reading the enum order in
# Shared/StaticDefinitions.hh PetalID and grouping consecutive
# entries that share a base name in PETAL_DATA.
PETAL_FAMILY: dict[int, str] = {
    0: "none",
    # ---- pre-wave base petals (ids match StaticDefinitions.hh order) ----
    1: "basic", 2: "basic", 3: "basic", 4: "basic",
    5: "light", 6: "light", 7: "light", 8: "light",
    9: "heavy", 10: "heavy", 11: "heavy", 12: "heavy", 13: "heavy",
    14: "stinger", 15: "stinger", 16: "stinger", 17: "stinger",
    18: "leaf", 19: "leaf", 20: "leaf", 21: "leaf", 22: "leaf",
    23: "twin",
    24: "rose", 25: "rose", 26: "rose", 27: "rose",
    28: "iris", 29: "iris", 30: "iris", 31: "iris",
    32: "missile",
    33: "dandelion",
    34: "bubble", 35: "bubble", 36: "bubble", 37: "bubble", 38: "bubble",
    39: "faster",
    40: "rock", 41: "rock", 42: "rock", 43: "rock", 44: "rock",
    45: "cactus",
    46: "web",
    47: "wing",
    48: "peas",
    49: "sand",
    50: "pincer",
    51: "dahlia",
    52: "triplet",
    53: "egg",        # AntEgg
    54: "iris",       # BlueIris — keep with iris family for matching
    55: "pollen",
    56: "peas",       # PoisonPeas — peas family
    57: "egg",        # BeetleEgg
    58: "azalea",
    59: "stick",
    60: "stinger", 61: "stinger",  # Tringer is the stinger-clump variant
    62: "web",        # Triweb
    63: "antennae",
    64: "cactus",     # Tricac
    65: "heavy",      # Heaviest
    66: "thirdeye",
    67: "observer",
    68: "cactus",     # PoisonCactus
    69: "salt",
    70: "basic",      # UniqueBasic — old special id, still basic family
    71: "square",
    72: "moon",
    73: "lotus",
    74: "cutter",
    75: "yinyang",
    76: "yggdrasil",
    77: "rice",
    78: "bone",
    79: "yucca",
    80: "corn",
    # ---- wave-system expansion (ids 81+) ----
    81: "basic", 82: "basic",                            # Legendary/Mythic Basic
    83: "light", 84: "light", 85: "light",               # Legendary/Mythic/Unique Light
    86: "heavy", 87: "heavy",                            # Mythic/Unique Heavy
    88: "stinger",                                       # UniqueStinger
    89: "leaf", 90: "leaf",                              # Mythic/Unique Leaf
    91: "twin", 92: "twin", 93: "twin", 94: "twin", 95: "twin", 96: "twin",
    97: "rose", 98: "rose",                              # Mythic/Unique Rose
    99: "iris", 100: "iris",                             # Mythic/Unique Iris
    101: "bubble", 102: "bubble",                        # Mythic/Unique Bubble
    103: "rock", 104: "rock",                            # Mythic/Unique Rock
    105: "web", 106: "web", 107: "web", 108: "web", 109: "web",
    110: "wing", 111: "wing", 112: "wing", 113: "wing", 114: "wing", 115: "wing",
    116: "peas", 117: "peas", 118: "peas", 119: "peas", 120: "peas",
    121: "sand", 122: "sand", 123: "sand", 124: "sand", 125: "sand", 126: "sand",
    127: "pincer", 128: "pincer", 129: "pincer", 130: "pincer", 131: "pincer", 132: "pincer",
    133: "dahlia", 134: "dahlia", 135: "dahlia", 136: "dahlia", 137: "dahlia", 138: "dahlia",
    139: "triplet", 140: "triplet", 141: "triplet", 142: "triplet", 143: "triplet", 144: "triplet",
    145: "salt", 146: "salt", 147: "salt", 148: "salt", 149: "salt", 150: "salt",
    151: "pollen", 152: "pollen", 153: "pollen", 154: "pollen", 155: "pollen", 156: "pollen",
    157: "faster", 158: "faster", 159: "faster", 160: "faster", 161: "faster", 162: "faster",
    163: "cactus", 164: "cactus", 165: "cactus", 166: "cactus",
    167: "missile", 168: "missile", 169: "missile", 170: "missile", 171: "missile", 172: "missile",
    173: "dandelion", 174: "dandelion", 175: "dandelion", 176: "dandelion", 177: "dandelion", 178: "dandelion",
    179: "yucca", 180: "yucca", 181: "yucca", 182: "yucca", 183: "yucca", 184: "yucca",
    185: "bone", 186: "bone", 187: "bone", 188: "bone", 189: "bone", 190: "bone",
    191: "rice", 192: "rice", 193: "rice", 194: "rice", 195: "rice", 196: "rice",
    197: "corn", 198: "corn", 199: "corn", 200: "corn", 201: "corn", 202: "corn",
    203: "peas",                                          # EpicPeas appended post-expansion
    204: "root", 205: "root", 206: "root", 207: "root", 208: "root", 209: "root", 210: "root",
    211: "yggdrasil", 212: "yggdrasil", 213: "yggdrasil", 214: "yggdrasil",
    215: "yggdrasil", 216: "yggdrasil",
}


def family_of(petal_id: int) -> str:
    """Return the rarity-agnostic family name for a PetalID. Unknown
    ids (e.g. ones added to the enum but not registered here) return
    "unknown" — they won't match anything and won't count against the
    bot's progress, just silently miss out on whatever family they
    belong to."""
    return PETAL_FAMILY.get(int(petal_id), "unknown")


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
    """
    if not target_families:
        return 0.0
    tc = Counter(target_families)
    cc = Counter(family_of(p) for p in current_petal_ids)
    matched = 0
    for fam, want in tc.items():
        matched += min(want, cc.get(fam, 0))
    return matched / len(target_families)
