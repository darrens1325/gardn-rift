"""Static-map collision data for the bots.

The server collides entities against a Tiled-format map at
`Map/main/main.tmj` (see `Server/TiledMap.cc`). The bot reads the same
file at startup so its state vector can encode "how much room is there
in each direction" — without this the policy has no way to learn wall
avoidance, only entity avoidance, and ends up grinding against
geometry.

Two obstacle sources are extracted:
  1. Object-layer "collision" rectangles — explicit AABBs hand-placed
     in the editor (3 in the current map).
  2. Solid-tile-layer occupied cells from layers named in `SOLID_LAYERS`
     (water / bush / cliff / dirt / castle). The server uses per-tile
     SVG polygons for the precise shape; we skip the SVG decode and
     treat each occupied cell as the full tile_w × tile_h rectangle.
     Coarse, but adequate for "is there terrain in this direction"
     sensing — the policy doesn't need pixel-accurate wall shapes.

Plus the implicit arena boundary at (0, 0) → (world_w, world_h),
derived from the .tmj's width × tilewidth and height × tileheight.

This module also extracts the map's warp (portal) circles, parsed the
same way `Server/TiledMap.cc:parse_warps_layer` does: object-layer
objects whose `type`/`class` is "warp". Each warp is a trigger circle
at (x, y) with radius = max(96, max(width, height) / 2). Exposed
through `load_warps` so the bot can encode portal proximity in its
observation (mirrors what a human player sees on the minimap)."""

from __future__ import annotations

import base64
import gzip
import json
import math
import os
import struct
import zlib
from typing import List, Tuple

SOLID_LAYERS = ("water", "bush", "cliff", "dirt", "castle")
DEFAULT_TMJ_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Map", "main", "main.tmj",
)

# (x, y, w, h) AABB in world coordinates.
Rect = Tuple[float, float, float, float]


def _decode_tile_layer(layer: dict) -> List[int]:
    """Decompress + base64-decode a tile layer's `data` field. Returns
    a list of GIDs in row-major order. Supports gzip and zlib
    compression (the two Tiled offers); raw / csv layouts return the
    list as-is."""
    data = layer.get("data")
    if isinstance(data, list):
        return list(data)
    if not isinstance(data, str):
        return []
    raw = base64.b64decode(data)
    comp = layer.get("compression")
    if comp == "gzip":
        raw = gzip.decompress(raw)
    elif comp == "zlib":
        raw = zlib.decompress(raw)
    return list(struct.unpack(f"<{len(raw) // 4}I", raw))


def load_walls(tmj_path: str = DEFAULT_TMJ_PATH) -> Tuple[float, float, List[Rect]]:
    """Parse a Tiled .tmj file. Returns (world_w, world_h, walls).

    Missing files return (0, 0, []) — the bot still runs, just without
    wall sensors, so a deployment that can't see the map (e.g. running
    against a server on a different machine where the .tmj wasn't
    copied) degrades gracefully instead of crashing.
    """
    if not os.path.exists(tmj_path):
        return 0.0, 0.0, []
    try:
        with open(tmj_path) as f:
            m = json.load(f)
    except (OSError, json.JSONDecodeError):
        return 0.0, 0.0, []
    tile_w = float(m.get("tilewidth", 512))
    tile_h = float(m.get("tileheight", 512))
    cols = int(m.get("width", 0))
    rows = int(m.get("height", 0))
    world_w = cols * tile_w
    world_h = rows * tile_h
    walls: List[Rect] = []

    for layer in m.get("layers", []):
        ty = layer.get("type")
        if ty == "tilelayer":
            if layer.get("name") not in SOLID_LAYERS:
                continue
            try:
                gids = _decode_tile_layer(layer)
            except (OSError, ValueError, struct.error):
                continue
            for i, gid in enumerate(gids):
                # Mask off the high flip bits; non-zero GID = occupied cell.
                if (gid & 0x1FFFFFFF) == 0:
                    continue
                col = i % cols
                row = i // cols
                walls.append((col * tile_w, row * tile_h, tile_w, tile_h))
        elif ty == "objectgroup":
            if layer.get("name") != "collision":
                continue
            for obj in layer.get("objects", []):
                w = float(obj.get("width", 0))
                h = float(obj.get("height", 0))
                if w > 0 and h > 0:
                    walls.append((float(obj["x"]), float(obj["y"]), w, h))

    return world_w, world_h, walls


# Cardinal directions for the wall sensor: N (up), E (right), S (down),
# W (left). The game's coordinate system matches screen-space: +y is
# down, so "N" is dy<0.
WALL_FEAT_DIM = 4

# Max distance reported by a single ray sensor. Beyond this the bot
# sees "fully clear" and doesn't differentiate further. Tuned to be
# meaningfully larger than the player's per-tick movement budget so the
# sensor is informative for planning a few ticks ahead.
WALL_RAY_CAP = 2000.0


def wall_ray_features(
    px: float, py: float, world_w: float, world_h: float, walls: List[Rect],
) -> List[float]:
    """Distances to the nearest wall (or arena boundary) along each of
    the 4 cardinal directions, normalised to [0, 1] where 1 = fully
    clear out to WALL_RAY_CAP and 0 = pressed against a wall.

    For cardinal rays the AABB intersection collapses to a 1-D check:
    a vertical ray hits a rect iff px is inside the rect's x-span,
    horizontal ray iff py is inside the rect's y-span. That makes this
    O(walls) per query, dominated by the solid-tile expansion (which
    can be a few hundred cells). No spatial index — the linear scan is
    fast enough in Python for the per-tick budget we care about.
    """
    # Start each ray at the arena-boundary distance, then minimise
    # over all walls.
    north = py if py > 0 else 0.0
    south = world_h - py if world_h > py else 0.0
    west = px if px > 0 else 0.0
    east = world_w - px if world_w > px else 0.0
    if world_w <= 0 or world_h <= 0:
        # No map loaded → bot just sees "fully clear in every direction",
        # which is the same signal as the original wall-blind state.
        north = south = east = west = WALL_RAY_CAP
    for (rx, ry, rw, rh) in walls:
        # Vertical rays (N/S): only walls whose x-span contains px.
        if rx <= px <= rx + rw:
            # North ray: walls strictly above the bot.
            if ry + rh <= py:
                d = py - (ry + rh)
                if d < north:
                    north = d
            # South ray: walls strictly below.
            elif ry >= py:
                d = ry - py
                if d < south:
                    south = d
        # Horizontal rays (E/W): only walls whose y-span contains py.
        if ry <= py <= ry + rh:
            if rx + rw <= px:
                d = px - (rx + rw)
                if d < west:
                    west = d
            elif rx >= px:
                d = rx - px
                if d < east:
                    east = d
    cap = WALL_RAY_CAP
    return [
        min(1.0, north / cap),
        min(1.0, east / cap),
        min(1.0, south / cap),
        min(1.0, west / cap),
    ]


# (x, y, radius) circle in world coordinates. Mirrors TiledWarp in
# Server/TiledMap.hh — the trigger is a circle around (x, y), not a
# rectangle, even though the .tmj stores width/height too.
Warp = Tuple[float, float, float]

# Minimum trigger radius the server applies even when a warp object
# has zero width/height. Matches the `std::max(96.0f, ...)` clamp in
# Server/TiledMap.cc:parse_warps_layer.
WARP_MIN_RADIUS = 96.0


def _object_kind(obj: dict) -> str:
    """Mirrors `object_kind` in Server/TiledMap.cc — Tiled stores a
    custom object class in either `type` (legacy) or `class` (current
    Tiled JSON), and the server falls back from one to the other."""
    t = obj.get("type")
    if isinstance(t, str) and t:
        return t
    c = obj.get("class")
    if isinstance(c, str):
        return c
    return ""


def load_warps(tmj_path: str = DEFAULT_TMJ_PATH) -> List[Warp]:
    """Parse the same .tmj `load_walls` reads and return every warp
    object's (x, y, radius). Missing/unparseable files return [], so
    the bot degrades gracefully (just sees no portals)."""
    if not os.path.exists(tmj_path):
        return []
    try:
        with open(tmj_path) as f:
            m = json.load(f)
    except (OSError, json.JSONDecodeError):
        return []
    out: List[Warp] = []
    for layer in m.get("layers", []):
        if layer.get("type") != "objectgroup":
            continue
        for obj in layer.get("objects", []):
            if _object_kind(obj) != "warp":
                continue
            try:
                x = float(obj.get("x", 0))
                y = float(obj.get("y", 0))
                w = float(obj.get("width", 0))
                h = float(obj.get("height", 0))
            except (TypeError, ValueError):
                continue
            radius = max(WARP_MIN_RADIUS, max(w, h) * 0.5)
            out.append((x, y, radius))
    return out


# Number of warps reported in the observation. Picking 2 keeps the
# feature vector compact while still letting the bot reason about a
# "nearer" + "next-nearer" portal pair on multi-warp maps.
K_WARPS = 2
# Features per warp slot: rel_dx_norm, rel_dy_norm, distance_norm,
# inside_trigger_flag. dx/dy use the same OBS_SCALE convention as
# hostile/drop features so the network sees a uniformly-scaled world.
WARP_FEAT_PER_SLOT = 4
WARP_FEAT_DIM = K_WARPS * WARP_FEAT_PER_SLOT

# Bot's normalised global position on the map: (px/world_w, py/world_h),
# both in [0, 1]. Mirrors what a player reads from the minimap — "I'm
# in the bottom-left corner of the world" — so the policy can learn
# coarse positional preferences (e.g. centre-of-map farming vs.
# corner-hugging). Plus a small flag set when no map is loaded so the
# zeros don't get mis-read as "top-left corner".
MINIMAP_FEAT_DIM = 3


def warp_features(
    px: float,
    py: float,
    warps: List[Warp],
    obs_scale: float,
) -> List[float]:
    """K_WARPS nearest warps, each as (rel_dx_norm, rel_dy_norm,
    distance_norm, inside_flag). Empty slots zero-pad. Distance is
    normalised by `obs_scale` and clamped to [0, 1] so the policy gets
    a uniform "near = small, far = saturating to 1" signal across
    feature columns."""
    if not warps:
        return [0.0] * WARP_FEAT_DIM
    scored: List[tuple[float, float, float, float]] = []
    for wx, wy, wr in warps:
        dx = wx - px
        dy = wy - py
        d2 = dx * dx + dy * dy
        scored.append((d2, dx, dy, wr))
    scored.sort(key=lambda s: s[0])
    out: List[float] = []
    for i in range(K_WARPS):
        if i < len(scored):
            d2, dx, dy, wr = scored[i]
            dist = math.sqrt(d2)
            out.extend([
                dx / obs_scale,
                dy / obs_scale,
                min(1.0, dist / obs_scale),
                1.0 if dist <= wr else 0.0,
            ])
        else:
            out.extend([0.0] * WARP_FEAT_PER_SLOT)
    return out


def minimap_features(
    px: float, py: float, world_w: float, world_h: float,
) -> List[float]:
    """Normalised global position: (px/world_w, py/world_h) ∈ [0, 1],
    plus a 1.0 sentinel when no map is loaded so the leading zeros
    aren't mistaken for "top-left corner". Same information a human
    player gets from the minimap dot."""
    if world_w <= 0 or world_h <= 0:
        return [0.0, 0.0, 1.0]
    return [
        max(0.0, min(1.0, px / world_w)),
        max(0.0, min(1.0, py / world_h)),
        0.0,
    ]
