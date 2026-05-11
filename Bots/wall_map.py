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
"""

from __future__ import annotations

import base64
import gzip
import json
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
