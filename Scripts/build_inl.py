#!/usr/bin/env python3
"""Parse florr.csv (manually transcribed gallery drop rates from florr.io)
and rewrite Shared/MobDropChancesBR.inl.

For each gardn mob mapped to a florr mob in MOB_MAP:
  1. Collect every (mob_rarity, drop_rarity, chance) data point from the
     florr CSV across all the florr mob's rarity views.
  2. Grid-search `n` in [0.05..1.0] to minimise sum-squared error of
     loot_table_gen(n)[mob_r][drop_r] vs the explicit chances.
  3. Emit one BR snapshot entry per drop in gardn's MOB_DATA[mob].drops:
     chance = loot_table_gen(n_fitted)[mob_authored_rarity][drop_rarity].
     If a primary-drop chance was explicitly given at the mob's authored
     rarity, prefer that exact value (override the formula).
"""
import csv
import math
import os
import re
import sys

ROOT = '/Users/darren/gardn'
CSV_PATH = os.path.join(ROOT, '.ocr_work', 'florr.csv')
INL_PATH = os.path.join(ROOT, 'Shared', 'MobDropChancesBR.inl')

MOB_MAP = {
    'Baby Ant':              'Baby Ant',
    'Worker Ant':            'Worker Ant',
    'Soldier Ant (Black)':   'Soldier Ant',
    'Bee':                   'Bee',
    'Ladybug':               'Massive Ladybug',
    'Ladybug (Red)':         'Ladybug',
    'Beetle (Gray)':         'Beetle',
    'Beetle (White)':        'Massive Beetle',
    'Hornet':                'Hornet',
    'Cactus':                'Cactus',
    'Rock':                  'Rock',
    'Centipede':             'Centipede',
    'Centipede (Purple)':    'Evil Centipede',
    'Centipede (Yellow)':    'Desert Centipede',
    'Sandstorm':             'Sandstorm',
    'Scorpion':              'Scorpion',
    'Spider':                'Spider',
    'Ant Hole':              'Ant Hole',
    'Queen Ant':             'Queen Ant',
    'Square':                'Square',
    'Digger':                'Digger',
    'Leafbug':               'Leafbug',
    'Bush':                  'Bush',
    'Mantis':                'Mantis',
    'Wasp':                  'Wasp',
}

PETAL_MAP = {
    'Stinger':   'Stinger',
    'Pollen':    'Pollen',
    'Rice':      'Rice',
    'Light':     'Light',
    'Leaf':      'Leaf',
    'Peas':      'Peas',
    'Iris':      'Iris',
    'Missile':   'Missile',
    'Antennas':  'Antennae',
    'Wing':      'Wing',
    'Bubble':    'Bubble',
    'Cactus':    'Cactus',
    'Heavy':     'Heavy',
    'Rock':      'Rock',
    'Sand':      'Sand',
    'Stick':     'Stick',
    'Dahlia':    'Dahlia',
    'Rose':      'Rose',
    'Yin Yang':  'YinYang',
    'Yggdrasil': 'Yggdrasil',
    'Bone':      'Bone',
    'Yucca':     'Yucca',
    'Corn':      'Corn',
    'Root':      'Root',
    'Basic':     'Basic',
    'Dandelion': 'Dandelion',
    'Pincer':    'Pincer',
    'Triplet':   'Triplet',
    'Cutter':    'Cutter',
    'Faster':    'Faster',
    'Web':       'Web',
    'Salt':      'Salt',
    'Ant Egg':   'AntEgg',
    'Square':    'Square',
}

RARITY_MAP = {
    'com': 0, 'common': 0,
    'unu': 1, 'unusual': 1,
    'rare': 2,
    'epic': 3,
    'leg': 4, 'legendary': 4,
    'mythic': 5, 'myth': 5,
    'ultra': 6, 'unique': 6, 'ult': 6,
}

NUM_RARITIES = 7

# Constants taken from Scripts/drop_constant.py loot_table_gen; the
# trimmed-to-7 version matches Shared/StaticData.cc's _loot_table_gen.
# Tuned by .ocr_work/tune_constants.py via Nelder-Mead fit against the
# 789 observations parsed from florr.csv. The fit shifts each mob's
# peak drop tier closer to the mob's own rarity (matches what mob_gallery
# actually shows). The original capped constants live in
# Shared/StaticData.cc's _loot_table_gen for NORMAL maps.
DROPS_BOUNDS = [
    0.0, 0.8280141620166028, 0.9994213801374269, 0.9999959838524433,
    0.9999999965213968, 0.9999999983920225, 0.9999999983920242, 1.0,
]
MOBS_DIVISOR = [
    24431.83968444404, 37424.40407882193, 530.0317372834454,
    144.8476629810749, 1.0432580614860787,
    0.000977177224351313, 0.0006446370750282694,
]


def loot_table_gen(n):
    """Mirror of Shared/StaticData.cc _loot_table_gen(n) (cap-free).
    Iterates drop_rarity 0..NUM_RARITIES-1 always so higher-tier drops
    get non-zero chance; matches florr's wider gallery distribution."""
    table = [[0.0] * NUM_RARITIES for _ in range(NUM_RARITIES)]
    last = NUM_RARITIES - 1
    for mob in range(NUM_RARITIES):
        exp = 300000.0 / MOBS_DIVISOR[mob]
        for drop in range(NUM_RARITIES):
            start = DROPS_BOUNDS[drop]
            end = 1.0 if drop == last else DROPS_BOUNDS[drop + 1]
            base1 = n * start + (1.0 - n)
            base2 = n * end + (1.0 - n)
            try:
                table[mob][drop] = (base2 ** exp) - (base1 ** exp)
            except OverflowError:
                table[mob][drop] = 0.0
    return table


def fit_n(data_points):
    """data_points = [(mob_rarity, drop_rarity, observed_chance), ...].
    Returns the n in [0.05..1.0] minimising sum-squared error vs the
    loot_table_gen prediction. Falls back to 0.5 if no usable points."""
    usable = [(mr, dr, c) for (mr, dr, c) in data_points
              if 0 <= mr < NUM_RARITIES and 0 <= dr < NUM_RARITIES]
    if not usable:
        return 0.5
    best_n, best_err = 0.5, 1e18
    for n10 in range(2, 21):  # 0.10 .. 1.00 in 0.05 steps
        n = n10 / 20.0
        table = loot_table_gen(n)
        err = 0.0
        for mr, dr, c in usable:
            pred = table[mr][dr]
            err += (pred - c) ** 2
        if err < best_err:
            best_err, best_n = err, n
    return best_n


def load_gardn_mob_data():
    src = open(os.path.join(ROOT, 'Shared', 'StaticData.cc')).read()
    out = {}
    mob_re = re.compile(
        r'"(?P<name>[A-Za-z][A-Za-z ]+)"\s*,\s*"[^"]*"\s*,\s*'
        r'RarityID::k(?P<rarity>[A-Z][a-z]+)\s*,'
        r'[^{}]*\{[^}]*\}\s*,'
        r'[^,]*,\s*'
        r'\{[^}]*\}\s*,\s*'
        r'\d+\s*,\s*'
        r'\{(?P<drops>[^}]*)\}',
        re.DOTALL,
    )
    rarity_to_idx = {'Common': 0, 'Unusual': 1, 'Rare': 2, 'Epic': 3,
                     'Legendary': 4, 'Mythic': 5, 'Unique': 6}
    for m in mob_re.finditer(src):
        rw = m.group('rarity')
        if rw not in rarity_to_idx:
            continue
        out[m.group('name')] = {
            'rarity': rarity_to_idx[rw],
            'drops': re.findall(r'PetalID::k([A-Z][A-Za-z]+)', m.group('drops')),
        }
    return out


def load_petal_data():
    src = open(os.path.join(ROOT, 'Shared', 'StaticDefinitions.hh')).read()
    out = {}
    decl_re = re.compile(
        r'inline\s+constexpr\s+Petal\s+k(?P<name>[A-Z][A-Za-z]+)\s*=\s*\{\s*'
        r'PetalType::k(?P<type>[A-Z][A-Za-z]+)\s*,\s*'
        r'RarityID::k(?P<rarity>[A-Z][a-z]+)\s*\}'
    )
    rarity_to_idx = {'Common': 0, 'Unusual': 1, 'Rare': 2, 'Epic': 3,
                     'Legendary': 4, 'Mythic': 5, 'Unique': 6}
    for m in decl_re.finditer(src):
        ridx = rarity_to_idx.get(m.group('rarity'))
        if ridx is None:
            continue
        out[m.group('name')] = (m.group('type'), ridx)
    return out


DROPS_PAREN_RE = re.compile(
    r'(?P<name>[A-Za-z][A-Za-z ]+?)\s*\((?P<contents>[^()]+)\)'
)
RARITY_PCT_RE = re.compile(r'(?P<rarity>[A-Za-z]+)\s+(?P<pct>\d+(?:\.\d+)?)\s*%')


def parse_drops(cell):
    out = []
    for grp in DROPS_PAREN_RE.finditer(cell):
        florr_name = grp.group('name').strip()
        for rp in RARITY_PCT_RE.finditer(grp.group('contents')):
            r = RARITY_MAP.get(rp.group('rarity').lower())
            if r is None:
                continue
            try:
                pct = float(rp.group('pct'))
            except ValueError:
                continue
            out.append((florr_name, r, pct / 100.0))
    return out


def main():
    gardn_mobs = load_gardn_mob_data()
    petals = load_petal_data()
    rarity_idx_to_word = ['Common', 'Unusual', 'Rare', 'Epic',
                          'Legendary', 'Mythic', 'Unique']

    # Per florr mob, collect rows: rarity → [(florr_petal, drop_rarity, chance)]
    by_mob = {}
    with open(CSV_PATH) as f:
        for row in csv.DictReader(f):
            florr_mob = row['Mob Name']
            r_word = row['Rarity'].split()[0].lower()
            r = RARITY_MAP.get(r_word)
            if r is None:
                continue
            by_mob.setdefault(florr_mob, {}).setdefault(r, []).extend(
                parse_drops(row['Primary Drops']))

    entries = []  # (mob_name, view_rarity, petal_type, petal_rarity, chance, tag)
    fit_log = []
    for florr_mob, gardn_name in MOB_MAP.items():
        if gardn_name not in gardn_mobs:
            print(f'WARN: gardn has no mob "{gardn_name}"', file=sys.stderr)
            continue
        info = gardn_mobs[gardn_name]
        rows = by_mob.get(florr_mob, {})
        if not rows:
            print(f'NOTE: no florr data for {florr_mob}', file=sys.stderr)
            continue

        # Collect data points + per-view explicit lookups.
        data_points = []                     # (mob_rarity, drop_rarity, chance) for n fit
        explicit_by_view = {v: {} for v in range(NUM_RARITIES)}
        for mr, drops in rows.items():
            for fname, dr, chance in drops:
                gardn_ptype = PETAL_MAP.get(fname)
                if gardn_ptype is None:
                    continue
                data_points.append((mr, dr, chance))
                if mr < NUM_RARITIES:
                    explicit_by_view[mr][(gardn_ptype, dr)] = chance

        n = fit_n(data_points)
        table = loot_table_gen(n)
        fit_log.append((florr_mob, gardn_name, n, len(data_points)))

        # Emit one entry per (gardn drop, view) pair.
        for view in range(NUM_RARITIES):
            for pid_name in info['drops']:
                pi = petals.get(pid_name)
                if not pi:
                    continue
                ptype, prarity = pi
                override = explicit_by_view[view].get((ptype, prarity))
                if override is not None:
                    chance = override
                    tag = 'explicit'
                else:
                    chance = table[view][prarity] if prarity < NUM_RARITIES else 0.0
                    tag = f'fit n={n:.2f}'
                entries.append((gardn_name, view, ptype, prarity, chance, tag))

    entries.sort(key=lambda e: (e[0], e[1], -e[4], e[2], e[3]))

    rarity_name = ['Common', 'Unusual', 'Rare', 'Epic',
                   'Legendary', 'Mythic', 'Unique']
    lines = [
        '// AUTO-GENERATED by .ocr_work/build_inl.py from .ocr_work/florr.csv.',
        '// Source: manual transcription of mob_gallery.mov primary-drop pairs.',
        '// One entry per (mob, view_rarity, petal_type, petal_rarity) cell;',
        '// the in-game gallery reads MOB_DROP_CHANCES_BR[mob][view] which is',
        '// built from these. Missing cells fall back to _loot_table_gen_br',
        '// at the view rarity via the sentinel in Shared/StaticData.cc.',
        'struct BRSnapshotEntry {',
        '    char const *mob_name;',
        '    uint8_t view_rarity;',
        '    char const *petal_name;',
        '    uint8_t petal_rarity;',
        '    float chance;',
        '};',
        f'static constexpr BRSnapshotEntry BR_DROP_SNAPSHOT[{len(entries)}] = {{',
    ]
    FLT_MIN = 1.175494351e-38  # smallest normal float; clamp tinier values up
    for mob, view, ptype, prarity, chance, tag in entries:
        if 0.0 < chance < FLT_MIN:
            chance = FLT_MIN
        c_str = f'{chance:.9g}'
        if '.' not in c_str and 'e' not in c_str:
            c_str += '.0'
        lines.append(f'    {{"{mob}", {view}, "{ptype}", {prarity}, {c_str}f}}, '
                     f'// view={rarity_name[view]} drop={rarity_name[prarity]} ({tag})')
    lines.append('};')
    with open(INL_PATH, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f'wrote {INL_PATH} with {len(entries)} entries')
    print('per-mob fit log (florr → gardn, n, n_datapoints):')
    for florr_mob, gardn_name, n, npts in fit_log:
        print(f'  {florr_mob:24} → {gardn_name:18} n={n:.2f}  pts={npts}')


if __name__ == '__main__':
    main()
