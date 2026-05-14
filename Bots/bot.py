"""A single learning bot for the gardn server.

Connection lifecycle is identical to the heuristic version: VERIFY → SPAWN →
loop(receive update, decide, send input). The decision is made by a shared
DQN agent (see agent.py); each bot just translates world state into
observation features, asks the agent for an action, and sends the matching
input packet.

Each tick we also push (s, a, r, s', done) into the agent's replay buffer:
  - s   = features at the previous decision
  - a   = action we took
  - r   = reward earned this tick
  - s'  = features now
  - done = True if the player died this tick

Reward shaping:
  - score delta within an episode (kills give big positive bumps)
  - +5 * hp delta (small reward for healing, small punishment for taking damage)
  - -0.01 idle penalty per tick (encourages action)
  - -10 terminal on death

`done` resets when we respawn; the score baseline is reset too so the
respawn XP grant doesn't get credited as a "reward".
"""

from __future__ import annotations

import asyncio
import math

import websockets

from agent import (
    COMM_PER_PEER,
    DQNAgent,
    DROP_FEAT_PER_SLOT,
    K_DROPS,
    K_PEERS,
    NUM_MOVEMENT_ACTIONS,
    OBS_SCALE,
    STATE_DIM,
    action_to_input,
    decode_inventory_action,
    is_movement_action,
)
from memory import EpisodicMemory, PersistentMemory
from wall_map import WALL_FEAT_DIM, load_walls, wall_ray_features
from protocol import (
    C_CHAT,
    C_CLIENT_UPDATE,
    C_OUTDATED,
    C_ROUND_END,
    INPUT_ATTACK,
    INPUT_DEFEND,
    MAX_SLOT_COUNT,
    PETAL_BASIC,
    PETAL_NONE,
    S_CLIENT_INPUT,
    S_CLIENT_SPAWN,
    S_PETAL_DELETE,
    S_PETAL_SWAP,
    S_STEP,
    S_VERIFY,
    Writer,
    build_chat_packet,
    decode_data_chat,
    encode_help,
    encode_kill,
    encode_position,
    has_component,
    parse_chat_packet,
    parse_client_update,
    parse_round_end,
    petal_burst_norm,
    petal_rank,
    petal_type_norm,
)

VERSION_HASH = 4728567265382327  # bumped: added Serverbound::kStep for sync mode

MOVE_MAG = 260.0          # server clamps to PLAYER_ACCELERATION above 200
DEFAULT_CONTROL_HZ = 20   # sane default for stock TPS=20; bump when server runs faster
NEAREST_K = 3             # number of hostiles encoded in the observation

# Reward weights
W_SCORE = 1.2
W_HP = 10.0
IDLE_PENALTY = 0.05
DEATH_PENALTY = 18.0

# PvP reward shaping. Encourages the policy to seek out other bots rather
# than only farming mobs.
#  - W_PVP_BONUS: extra multiplier *only* on damage credit attributed to
#    enemy Flower entities (player-vs-player damage we measured via petal
#    overlap on the snapshot — see the credit loops in `_decide_and_learn`).
#    Player damage pays `W_DAMAGE × (1 + W_PVP_BONUS)` per HP-ratio point;
#    mob damage pays the base `W_DAMAGE`. We do not gate on score deltas
#    or any other client-side prediction: the only thing that counts as
#    "PvP" is a petal of ours having overlapped a hostile Flower's body
#    when its HP went down.
#  - W_PROXIMITY / PROXIMITY_RANGE: small per-tick bonus while at least one
#    hostile (mob or player) is within range. Continuous gradient toward
#    "be near a fight" even before a kill lands. Tiny per tick.
W_PVP_BONUS = 5.0
W_PROXIMITY = 0.05
PROXIMITY_RANGE = 600.0

# Damage-dealt shaping. The proximity bonus rewards loitering near an enemy;
# this rewards actually *hitting* one. Every tick we snapshot every visible
# enemy player's health_ratio. On the next tick, if a snapshotted enemy's HP
# dropped *and* they're within HIT_RANGE of us *and* our previous action had
# the attack flag held, we credit it as our damage. The credit can leak to
# us when a mob attacks the same target simultaneously, but the gradient
# direction (be in petal-overlap range + attack + enemy HP drops → reward)
# is exactly what we want the policy to learn.
W_DAMAGE = 10.0         # per HP-ratio point of damage attributed to us
# Additional reward stacked on top of W_DAMAGE for damage dealt by *our
# petals*. Every credited damage point in `_decide_and_learn` already comes
# from a confirmed petal-vs-enemy overlap (mob or player), so this bonus
# simply scales the petal-damage signal further. Effective per HP-ratio
# weight after stacking:
#   mob:    W_DAMAGE + W_PETAL_DAMAGE_BONUS                     (= 16.0)
#   player: W_DAMAGE × (1 + W_PVP_BONUS) + W_PETAL_DAMAGE_BONUS (= 32.0)
# The PvP multiplier still only multiplies the W_DAMAGE component so the
# new bonus doesn't compound with it; tune the bonus alone to control
# "how much do we want to encourage *any* petal-on-flesh contact."
W_PETAL_DAMAGE_BONUS = 8.0

# Approach shaping. The proximity / damage / kill rewards only pay out when
# bots are already *engaging*; if a bot spawns far from anything it gets no
# gradient until it stumbles into someone, which random exploration does
# very slowly. This term pays a small reward each tick proportional to how
# much the nearest-hostile distance shrunk vs. last tick — so any motion
# toward any target is rewarded directly.
#
# APPROACH_CAP filters out target-swap noise: when we kill the nearest
# hostile and the next-nearest is much further away, the raw distance jumps,
# which without a cap would register as a "moved away" penalty and punish
# the kill. Capping the per-tick contribution at a physically-plausible
# closing speed (≈ terminal velocity × a couple of ticks) keeps the term
# focused on actual motion rather than lock-target changes.
W_APPROACH = 2.0
APPROACH_CAP = 50.0    # max distance-units of closing rewarded per tick

# Petal-having shaping. With 24 inventory actions out of 42, a uniform-
# random ε-greedy policy picks delete/swap on more than half of all ticks,
# which strips the loadout to kNone within seconds — and then the bot
# tries to attack things with no petals and just dies. The deletion's
# consequence (no damage credit, no kills) is many ticks delayed, so the
# DQN takes a long time to learn it without help.
#
# This term pays per-tick reward proportional to "non-empty active primary
# slots" — every petal you keep alive in slots [0..loadout_count-1] earns
# `W_PETAL_PRESENT` per control tick. Deleting a useful petal immediately
# reduces this reward stream, giving the policy the missing direct gradient.
# Keeps inventory actions model-controlled (no heuristic gating); just
# makes the cost of deletion immediately visible.
W_PETAL_PRESENT = 0.1
# Per-tick bonus is scaled by `1 + RARITY_PRESENT_SCALE * rank` so a slot
# holding kUnique pays more than a slot holding kCommon. With scale=0.5:
# common=1.0×, unusual=1.5×, rare=2.0×, ..., unique=4.0×. Gives the policy
# an immediate, monotonic signal that rarer petals are more valuable to keep
# (and conversely more valuable to swap *into* the primary row from a drop).
# Without this the petal-present stream was flat — the only way the model
# could tell rare from common was via downstream damage credit, which is
# delayed by many ticks and shared with mob damage noise.
RARITY_PRESENT_SCALE = 0.5
# Active *negative* reward per empty primary slot per tick. Combined with
# W_PETAL_PRESENT this turns the "keep your petals" signal into a two-sided
# gradient: full loadout pays the positive bonus, deletes immediately incur
# a per-tick cost. Without the penalty, the worst case (zero petals) was
# just "no positive reward" — same as standing still — and the policy had
# no direct cost signal for staying empty after a delete spree.
W_EMPTY_PRIMARY_PENALTY = 0.25
# Per-tick value of a non-empty *storage* slot, expressed as a multiplier
# on the primary slot's W_PETAL_PRESENT. Storage petals can't damage anyone
# (they only orbit once swapped into primary), so we don't pay them the
# full primary rate — but we *do* pay them something so the policy has a
# gradient to fetch drops even when its primary row is already full.
# Without this term, `_petal_having` rewarded only primary slots, and an
# Epic petal lying on the ground next to a level-1 bot with a full primary
# row was worth zero per-tick → the policy never bothered walking over it.
# Half the primary rate keeps "fill primary first" preferred without
# making storage worthless.
W_PETAL_STORAGE_RATIO = 0.5
# One-shot penalty that fires the tick we send a kPetalDelete packet —
# attached to the *action*, not the resulting state, so the credit
# flows back to the action that caused it without waiting for the
# delayed empty-slot stream to bite. Big enough to discourage random
# delete-spam during ε-greedy exploration; small enough that an actually
# useful delete (storage-full + replacing trash with a drop) still
# pays off via the better-petal reward later.
#
# Scaled by the rarity of the deleted slot — deleting a kUnique is far
# more punishing than deleting a kCommon. The rank is read from the
# previous-state loadout column (loadout_feats start at offset 13).
W_DELETE_COST = 1.5
# Slot 0 of the loadout-rank feature column lives here in `_prev_state`
# (1 hp + 12 hostile = 13). Used to scale W_DELETE_COST by the rarity of
# whatever lived in the slot we just deleted from.
LOADOUT_RANK_OFFSET = 13

# Kill-event bonuses. The damage-credit path already pays per HP-ratio
# point dealt by our petals, so the killing blow naturally pays whatever
# share of the victim's HP we removed in that final tick. These flat
# bonuses sit *on top* of that and fire exactly once per confirmed kill
# (snapshotted entity disappeared with at least one snapshot petal
# overlapping its last-known position) — measured, not predicted. The
# asymmetry encodes "killing another bot is worth a lot more than killing
# a mob," because mobs are everywhere and players are scarce.
W_MOB_KILL_BONUS = 10.0
W_PLAYER_KILL_BONUS = 50.0

# Wave-system end-of-round bonus. Server fires kRoundEnd every
# WAVE_TICKS_PER_ROUND game-ticks naming the player with the highest score
# at the moment of reset. Whichever bot's name matches gets W_ROUND_WIN
# credited into the same transition that would otherwise carry the round-
# end DEATH_PENALTY — net effect: winning a round pays roughly
# (W_ROUND_WIN - DEATH_PENALTY) ≈ +32, losing a round pays -DEATH_PENALTY.
# Sized to outweigh the death penalty by a clear margin so the policy
# can learn "max score by tick 72000" as a top-level objective without
# the death cost cancelling the win.
W_ROUND_WIN = 150.0


def _hostile_features(entities: dict, my_player: dict, my_team: tuple[int, int]) -> list[float]:
    """Return a flat list of NEAREST_K * 4 features for the K closest hostiles."""
    px, py = my_player["x"], my_player["y"]
    my_id = my_player["_id"]
    cands = []
    for eid, ent in entities.items():
        if eid == my_id:
            continue
        if "x" not in ent:
            continue
        if ent.get("_pending_delete"):
            continue
        is_flower = has_component(ent, "Flower")
        is_mob = has_component(ent, "Mob")
        if not (is_flower or is_mob):
            continue
        if ent.get("team", (0, 0)) == my_team:
            continue
        dx = ent["x"] - px
        dy = ent["y"] - py
        d2 = dx * dx + dy * dy
        cands.append((d2, dx, dy, 1.0 if is_flower else 0.0, float(ent.get("health_ratio", 1.0))))
    cands.sort(key=lambda t: t[0])
    out: list[float] = []
    for i in range(NEAREST_K):
        if i < len(cands):
            _, dx, dy, ip, hp = cands[i]
            out.extend([dx / OBS_SCALE, dy / OBS_SCALE, ip, hp])
        else:
            out.extend([0.0, 0.0, 0.0, 0.0])
    return out


def _nearest_hostile_dist_sq(
    entities: dict,
    my_player: dict,
    my_team: tuple[int, int],
) -> float:
    """Squared distance to the closest hostile in view, mobs *and* enemy
    players. Used by the per-tick approach reward so the policy gets a
    gradient toward any target, not just other bots."""
    px, py = my_player["x"], my_player["y"]
    my_id = my_player["_id"]
    best = math.inf
    for eid, ent in entities.items():
        if eid == my_id:
            continue
        if "x" not in ent or ent.get("_pending_delete"):
            continue
        is_flower = has_component(ent, "Flower")
        is_mob = has_component(ent, "Mob")
        if not (is_flower or is_mob):
            continue
        if ent.get("team", (0, 0)) == my_team:
            continue
        dx = ent["x"] - px
        dy = ent["y"] - py
        d2 = dx * dx + dy * dy
        if d2 < best:
            best = d2
    return best


_MAX_RARITY_RANK = 6  # RARITY_UNIQUE — see protocol.py PETAL_RARITY


def _loadout_features(my_player: dict) -> list[float]:
    """One float per loadout slot, encoding what petal lives there.
    Empty slots map to 0.0. Non-empty slots map to (rank+1)/(_MAX_RARITY_RANK+1)
    so kCommon ≈ 0.14, kUnique = 1.0 — and a kCommon slot is clearly
    distinct from an empty slot. Without this the network has no per-slot
    info and picks swap/delete blindly; with it, the model can learn
    'storage[3] is rarer than primary[2], swap them.'
    """
    out = [0.0] * 16  # 2 * MAX_SLOT_COUNT
    ids = my_player.get("loadout_ids")
    if not ids:
        return out
    n = min(16, len(ids))
    for i in range(n):
        pid = int(ids[i])
        if pid == PETAL_NONE:
            continue  # leave 0.0 (means empty)
        rank = petal_rank(pid)
        if rank < 0:
            continue
        out[i] = (rank + 1) / (_MAX_RARITY_RANK + 1)
    return out


def _loadout_burst_features(my_player: dict) -> list[float]:
    """One float per loadout slot, encoding the petal's effective burst
    (damage × count, normalised). Lets the network see *absolute power*
    of a slot — `kEpicHeavy` (0.467) beats `kEpicLight` (0.233) even
    though both are rank=Epic + type=damage. Empty slots map to 0.0."""
    out = [0.0] * 16
    ids = my_player.get("loadout_ids")
    if not ids:
        return out
    n = min(16, len(ids))
    for i in range(n):
        pid = int(ids[i])
        if pid == PETAL_NONE:
            continue
        out[i] = petal_burst_norm(pid)
    return out


def _loadout_type_features(my_player: dict) -> list[float]:
    """One float per loadout slot, encoding the petal's *role* (damage /
    tank / heal / poison / utility). Empty slots map to 0.0. Pairs with
    `_loadout_features` (rarity rank) so the network sees both 'how rare
    is this slot' and 'what does it do'."""
    out = [0.0] * 16
    ids = my_player.get("loadout_ids")
    if not ids:
        return out
    n = min(16, len(ids))
    for i in range(n):
        pid = int(ids[i])
        if pid == PETAL_NONE:
            continue
        out[i] = petal_type_norm(pid)
    return out


def _drop_features(entities: dict, my_player: dict) -> list[float]:
    """K_DROPS nearest ground drops, each as
        (rel_dx_norm, rel_dy_norm, drop_rank_norm, drop_type_norm).
    Empty slots are zero-padded. The drop's `drop_id` field tells us what
    petal we'd pick up, so we can score the drop the same way a slot is
    scored — bot can navigate toward useful drops, ignore basics."""
    px = float(my_player.get("x", 0.0))
    py = float(my_player.get("y", 0.0))
    cands: list[tuple[float, float, float, int]] = []  # (d2, dx, dy, drop_id)
    for eid, ent in entities.items():
        if not has_component(ent, "Drop"):
            continue
        if "x" not in ent or ent.get("_pending_delete"):
            continue
        dx = float(ent["x"]) - px
        dy = float(ent["y"]) - py
        d2 = dx * dx + dy * dy
        cands.append((d2, dx, dy, int(ent.get("drop_id", 0))))
    cands.sort(key=lambda t: t[0])
    out: list[float] = []
    for i in range(K_DROPS):
        if i < len(cands):
            _, dx, dy, drop_id = cands[i]
            rank = petal_rank(drop_id)
            rank_norm = (rank + 1) / (_MAX_RARITY_RANK + 1) if rank >= 0 else 0.0
            out.extend([
                dx / OBS_SCALE,
                dy / OBS_SCALE,
                rank_norm,
                petal_type_norm(drop_id),
            ])
        else:
            out.extend([0.0] * DROP_FEAT_PER_SLOT)
    return out


def _build_state(
    my_player: dict,
    hostile_feats: list[float],
    loadout_feats: list[float],
    peer_feats: list[float],
    loadout_type_feats: list[float],
    drop_feats: list[float],
    loadout_burst_feats: list[float],
    wall_feats: list[float],
) -> list[float]:
    # Append-only layout. Indices [0..40] match the original 41-input
    # version, [0..68] match the type+drops version, [69..84] are the
    # per-slot effective-burst column, [85..88] are the wall-ray
    # sensors. Older checkpoints pad-load cleanly into the leading
    # slots; new training fills the trailing wall columns.
    return (
        [float(my_player.get("health_ratio", 1.0))]
        + hostile_feats
        + loadout_feats
        + peer_feats
        + loadout_type_feats
        + drop_feats
        + loadout_burst_feats
        + wall_feats
    )


def _peer_features(agent: DQNAgent, my_name: str, my_player: dict) -> list[float]:
    """Read the K nearest peer messages from the agent's blackboard and
    convert them into a flat feature vector. Each slot is COMM_PER_PEER:

        rel_dx_norm, rel_dy_norm, peer_hp, peer_vx, peer_vy, peer_attacking

    `peer_vx/peer_vy` are the unit-direction components of the peer's most
    recent chosen action (decoded the same way our own action is decoded
    server-bound), so this slot literally is "what action did that peer's
    QNet output, mapped into motion-space."
    """
    px = float(my_player.get("x", 0.0))
    py = float(my_player.get("y", 0.0))
    peers = agent.read_peers(my_name, px, py)
    out: list[float] = []
    for i in range(K_PEERS):
        if i < len(peers):
            p = peers[i]
            dx = float(p.get("x", 0.0)) - px
            dy = float(p.get("y", 0.0)) - py
            # Clamp inventory actions (>= NUM_MOVEMENT_ACTIONS) to "stay" in
            # the comm vector. Inventory actions don't move the peer, so the
            # peer_vx/peer_vy slot should report zeros — passing the raw
            # index into action_to_input would compute a bogus direction.
            peer_action = int(p.get("action", 0))
            if peer_action >= NUM_MOVEMENT_ACTIONS:
                peer_action = 0
            ax, ay, flags = action_to_input(
                peer_action, 1.0, INPUT_ATTACK, INPUT_DEFEND
            )
            out.extend([
                dx / OBS_SCALE,
                dy / OBS_SCALE,
                float(p.get("hp", 1.0)),
                float(ax),
                float(ay),
                1.0 if (flags & INPUT_ATTACK) else 0.0,
            ])
        else:
            out.extend([0.0] * COMM_PER_PEER)
    return out


# Static wall map, loaded once per process and shared across all bots
# in this worker. `load_walls` returns (0, 0, []) if the .tmj is
# missing — in that case `wall_ray_features` reports "fully clear" in
# every direction, matching the pre-wall behavior so a bot run without
# the map still functions (just won't learn wall avoidance).
_WALL_WORLD_W, _WALL_WORLD_H, _WALL_RECTS = load_walls()
print(f"[wall_map] loaded {len(_WALL_RECTS)} wall rect(s); world bounds "
      f"{_WALL_WORLD_W:.0f} × {_WALL_WORLD_H:.0f}", flush=True)
_ZERO_WALL_FEATS = [0.0] * WALL_FEAT_DIM

_ZERO_STATE = [0.0] * STATE_DIM


class LearningBot:
    # Class-level flag, set by run.py at startup. When True, the bot
    # bypasses the round-end respawn gate and re-enters the game
    # immediately on death. Trades the "all respawn together" wave
    # mechanic for ~100× higher episode rate during early training.
    respawn_immediately = False
    # Class-level flag, set by run.py at startup when --sync is passed.
    # In sync mode the control loop runs lockstep with the server: the
    # bot sends S_STEP after each input, then awaits the next world
    # update before deciding again. The wall-clock `control_hz` pacing
    # is disabled — both sides run as fast as the slower one can manage.
    sync_mode = False

    def __init__(
        self,
        name: str,
        agent: DQNAgent,
        url: str = "ws://localhost:9001",
        control_hz: float = DEFAULT_CONTROL_HZ,
        action_repeat: int = 1,
    ) -> None:
        self.name = name
        self.agent = agent
        self.url = url
        self.control_hz = float(control_hz)
        # Hold each agent decision for `action_repeat` ticks before consulting
        # the policy again. Helps DQN credit assignment when the simulation is
        # running far faster than meaningful game-events take to resolve
        # (e.g. petal reload). 1 = every tick is a fresh decision.
        self.action_repeat = max(1, int(action_repeat))
        self._repeat_left = 0
        self._held_action: int | None = None

        # CPU-budget watchdog. The control loop is supposed to wake up
        # every `1/control_hz` seconds; when work per tick approaches that
        # budget the asyncio scheduler can't sustain the configured rate
        # and the realised env_step rate quietly drops below what was
        # asked for. These counters track that without spamming the log.
        # Read & reset via `timing_snapshot()`.
        self._tick_count = 0
        self._tick_overrun_count = 0   # ticks where work > control period
        self._tick_work_sum = 0.0      # cumulative work seconds
        self._tick_max_work = 0.0      # worst single tick this window
        # Severe-overrun warning is rate-limited to once every few seconds
        # so a brief CPU stall doesn't flood the log.
        self._last_severe_warn_time = 0.0
        self.entities: dict[tuple[int, int], dict] = {}
        self.camera_id: tuple[int, int] | None = None
        # Mirror of the server's Arena (leaderboard / player_count). Updated
        # on every kClientUpdate trailer so the bot has the same scoreboard
        # view a real player would render.
        self.arena: dict = {
            "player_count": 0,
            "scores": [0.0] * 10,
            "names": [""] * 10,
            "colors": [0] * 10,
        }

        # Episode tracking
        self._prev_state: list[float] | None = None
        self._prev_action: int | None = None
        self._prev_player_id: tuple[int, int] | None = None
        self._prev_score: int = 0
        self._prev_hp: float = 1.0
        self.episode_reward = 0.0
        self.episode_len = 0
        self.episodes_finished = 0
        self.last_episode_reward = 0.0
        self.last_episode_score = 0

        # Inventory diagnostics. Counts of model-driven swap / delete actions
        # actually sent this run (not capped: the model is in charge of when
        # to fire them, so a high count means the policy chose to). No more
        # heuristic throttle — the agent decides every tick whether to do
        # inventory or not, and one-shot semantics on inventory actions
        # means each pick translates to exactly one server packet.
        self.swaps_sent = 0
        self.deletes_sent = 0
        # Set by `_decide_and_learn` whenever the agent's chosen action is in
        # the inventory range. The control loop reads it, sends the matching
        # swap/delete packet, then clears it. None for movement actions.
        self._pending_inventory_action: tuple[str, int, int] | None = None

        # Engagement diagnostics. Most of these now count any hostile (mob
        # or player) — only `pvp_kills` is still PvP-specific because the
        # threshold-based PvP-kill bonus is the only term that's player-only.
        # `damage_dealt` resets on death so it reflects the *current* life;
        # `damage_dealt_lifetime` accumulates across deaths so the swarm-
        # total stays meaningful when bots die quickly.
        self.pvp_kills = 0
        self.engagement_ticks = 0
        self.damage_dealt = 0.0
        self.damage_dealt_lifetime = 0.0
        # Per-life counters fed to agent.record_episode_extras on death.
        # Cumulative-lifetime versions live above (pvp_kills,
        # damage_dealt_lifetime); these reset each life so the rolling
        # window run.py reads is a mean of single-episode performance,
        # not a swarm-total trend that monotonically increases.
        #
        # `_prev_nonempty_slots` is the per-tick snapshot used to detect
        # pickups: when count(loadout_ids != kNone) increases between
        # ticks, that delta is credited as drops_picked_up_this_life.
        # Swaps don't change the count so they don't false-positive.
        self.pvp_kills_this_life = 0
        self.drops_picked_up_this_life = 0
        self._prev_nonempty_slots = 0

        # Snapshot of last-seen (hp, x, y, radius) per hostile EntityID. Used
        # for damage attribution against per-petal collision. Position is
        # included because between two of our control-loop ticks an enemy
        # can vanish from `self.entities` (server pops deleted entities one
        # update after they died), so the bot's last view of the dead entity
        # is in this snapshot, not in `entities`. Without snapshotted
        # position we can't check petal overlap at the moment of death.
        # Reset on death so we don't credit cross-life damage.
        self._enemy_hp_snapshot: dict[tuple[int, int], tuple[float, float, float, float, bool]] = {}
        # Snapshot of our petals' (x, y, radius) at the same tick the enemy
        # snapshot was taken. Used for the disappearance-credit path: when
        # an enemy vanishes we want to check overlap against the petals
        # that *were there* when the snapshot was taken, not against current
        # petal positions which may have rotated to a different angle.
        self._my_petals_snapshot: list[tuple[float, float, float]] = []

        # Last tick's nearest-hostile distance, used for the approach reward
        # (closing-rate gradient). None outside of an active life.
        self._prev_nearest_hostile_dist: float | None = None
        self.approach_reward_total = 0.0  # diagnostic: cumulative this life

        # Per-bot memory. Persistent state lives across runs in
        # Bots/state/<name>.json; episodic state is re-created per life.
        self.persistent = PersistentMemory(name)
        self.episodic = EpisodicMemory()
        # Local chat throttle (in addition to the server's per-tick
        # CHAT_COOLDOWN). Wall-clock seconds between bot-emitted lines.
        self._chat_cooldown_s = 2.5

        # Wave / round state — driven by the server's kRoundEnd packets.
        #  - `_can_respawn` gates kClientSpawn sends. True initially (so the
        #    very first life starts immediately on connect), set False when
        #    the bot dies, and re-armed only by a kRoundEnd. Net result: a
        #    bot that dies mid-round sits in spectator state until the round
        #    ends, then everyone respawns together at round start.
        #  - `_round_win_pending` carries the kRoundEnd winner-bonus into
        #    the next reward credit. Folded into either the next normal
        #    transition's reward or the next death-credit, whichever fires
        #    first; cleared after use so each round-end pays exactly once.
        #  - `rounds_won` counts cumulative wins across the run for the
        #    stats line.
        self._can_respawn = True
        self._round_win_pending = 0.0
        self.rounds_won = 0
        self.rounds_seen = 0

    # -- protocol helpers (unchanged from the heuristic version) ------------

    async def _send_verify(self, ws) -> None:
        w = Writer(); w.w_u8(S_VERIFY); w.w_u64(VERSION_HASH)
        await ws.send(w.to_bytes())

    async def _send_spawn(self, ws) -> None:
        w = Writer(); w.w_u8(S_CLIENT_SPAWN); w.w_string(self.name[:16])
        await ws.send(w.to_bytes())

    async def _send_input(self, ws, x: float, y: float, flags: int) -> None:
        w = Writer(); w.w_u8(S_CLIENT_INPUT); w.w_float(x); w.w_float(y); w.w_u8(flags)
        await ws.send(w.to_bytes())

    async def _send_petal_swap(self, ws, pos1: int, pos2: int) -> None:
        w = Writer(); w.w_u8(S_PETAL_SWAP); w.w_u8(pos1); w.w_u8(pos2)
        await ws.send(w.to_bytes())

    async def _send_petal_delete(self, ws, pos: int) -> None:
        w = Writer(); w.w_u8(S_PETAL_DELETE); w.w_u8(pos)
        await ws.send(w.to_bytes())

    async def _send_step(self, ws) -> None:
        """Sync-mode pacer: tells the server we're ready for the next
        tick. A no-op on the server in wall-clock mode, so this is safe
        to call unconditionally — but `control_loop` only emits it
        when `sync_mode` is set, since otherwise every step request
        would be wasted bandwidth."""
        w = Writer(); w.w_u8(S_STEP)
        await ws.send(w.to_bytes())

    # -- watchdog ---------------------------------------------------------

    def timing_snapshot(self) -> dict:
        """Return CPU-budget counters since the last call, then reset.
        The stats loop reads this once per `--stats-interval` to compute
        realised vs configured tick rates."""
        snap = {
            "tick_count": self._tick_count,
            "overrun_count": self._tick_overrun_count,
            "work_sum_s": self._tick_work_sum,
            "max_work_s": self._tick_max_work,
            "configured_hz": self.control_hz,
        }
        self._tick_count = 0
        self._tick_overrun_count = 0
        self._tick_work_sum = 0.0
        self._tick_max_work = 0.0
        return snap

    def _record_tick_work(self, work_s: float, period_s: float) -> None:
        """Called once per control tick with how long the tick's work took.
        Updates rolling counters and emits an immediate warning for severe
        overruns (>2× period) — rate-limited so a brief CPU spike doesn't
        flood the log."""
        self._tick_count += 1
        self._tick_work_sum += work_s
        if work_s > self._tick_max_work:
            self._tick_max_work = work_s
        if work_s > period_s:
            self._tick_overrun_count += 1
        # Severe overrun: tick took >2× the budget. Print at most every 5
        # wall-seconds so 200 Hz × 4 bots can't drown the console.
        if work_s > 2 * period_s:
            now = asyncio.get_event_loop().time()
            if now - self._last_severe_warn_time > 5.0:
                self._last_severe_warn_time = now
                print(
                    f"[{self.name}] CPU overrun: tick took {work_s*1000:.1f} ms "
                    f"(budget {period_s*1000:.1f} ms at {self.control_hz:g} Hz). "
                    f"Realised rate is below configured.",
                    flush=True,
                )

    def _respawn_quip(self) -> str | None:
        """Pull from persistent stats. Skipped on the first life (when there's
        nothing interesting to say). Different shape every couple of respawns
        so the channel doesn't fill with the same line."""
        eps = self.persistent.episodes
        if eps == 0:
            return None
        rotation = eps % 4
        if rotation == 0:
            return f"back. {eps} lives so far"
        if rotation == 1:
            return f"avg score {self.persistent.avg_score:.1f} over {eps}"
        if rotation == 2 and self.persistent.score_best > 0:
            return f"pb {self.persistent.score_best}, beating it today"
        top = self.persistent.killed_by_top(1)
        if top and top[0][1] >= 2:
            who, n = top[0]
            return f"watching for {who} ({n}× killer)"
        return None

    async def _send_chat(self, ws, text: str, *, kind: str = "text") -> None:
        """Local rate-limit + cooldown bookkeeping. The server enforces its
        own CHAT_COOLDOWN_TICKS; this one just keeps a single bot from
        spamming faster than 1/2.5s wall-clock. `kind` is a label (e.g.
        'position', 'kill', 'help', 'text') used for per-kind last-sent
        bookkeeping so callers can throttle each kind independently."""
        if not text:
            return
        now = asyncio.get_event_loop().time()
        if now - self.episodic.last_chat_sent < self._chat_cooldown_s:
            return
        self.episodic.last_chat_sent = now
        self.episodic.last_data_sent[kind] = now
        try:
            await ws.send(build_chat_packet(text))
        except websockets.ConnectionClosed:
            pass

    # -- world queries ------------------------------------------------------

    def _my_camera(self) -> dict | None:
        if self.camera_id is None:
            return None
        return self.entities.get(self.camera_id)

    def _my_player(self) -> dict | None:
        cam = self._my_camera()
        if cam is None:
            return None
        pid = cam.get("player")
        if not pid or pid[0] == 0:
            return None
        ent = self.entities.get(pid)
        if ent is None or ent.get("_pending_delete") or "x" not in ent:
            return None
        return ent

    # -- learning step ------------------------------------------------------

    def _on_death(self, terminal_state: list[float]) -> None:
        if self._prev_state is not None and self._prev_action is not None:
            # If a kRoundEnd just fired naming us the winner, fold the
            # bonus into the same transition. Cleared after use so a single
            # round-end credits exactly once even if the bot's death and
            # the round-end coincide.
            terminal_reward = -DEATH_PENALTY + self._round_win_pending
            self._round_win_pending = 0.0
            self.episode_reward += terminal_reward
            self.agent.push(self._prev_state, self._prev_action, terminal_reward,
                            terminal_state, True)
        # NOTE: _can_respawn is *not* touched here. It is True at boot, set
        # False only after a successful spawn, and re-armed True only by a
        # kRoundEnd packet. Mid-round deaths leave the bot dead-without-
        # respawn until the round ends; round-end deaths see _can_respawn
        # already flipped True by the recv loop so the next control tick
        # respawns immediately.
        self.last_episode_reward = self.episode_reward
        self.last_episode_score = self._prev_score
        self.episodes_finished += 1
        # Feed the agent's rolling window — that's the curve to watch for
        # actual learning progress.
        self.agent.record_episode(self._prev_score, self.episode_reward)

        # Persist this episode to disk. We try to attribute the death to
        # whoever the camera says killed us (camera.killed_by is a player
        # name string set by the server when a player kill occurs).
        killed_by = None
        cam = self._my_camera()
        if cam is not None:
            kb = cam.get("killed_by")
            if isinstance(kb, str) and kb:
                killed_by = kb
        primary_loadout = list(self.episodic.last_primary_loadout)

        # Auxiliary per-episode metrics so run.py can maintain separate
        # `<base>.best.<metric>` checkpoints alongside the score-based
        # one. `damage` is read from this life's accumulator (it'll be
        # reset to 0.0 a few lines down); `petal_rarity` is the mean rank
        # of the non-empty primary slots at the moment of death, so a
        # bot that died with a kEpic + kRare loadout scores higher than
        # one that died bare-handed. Must read `primary_loadout` after
        # it's been assigned above — keep this block below that line.
        avg_rarity = 0.0
        if primary_loadout:
            ranks = [petal_rank(int(p)) for p in primary_loadout]
            ranks = [r for r in ranks if r >= 0]
            if ranks:
                avg_rarity = sum(ranks) / len(ranks)
        self.agent.record_episode_extras({
            "kills": float(self.pvp_kills_this_life),
            "drops": float(self.drops_picked_up_this_life),
            "damage": float(self.damage_dealt),
            "petal_rarity": avg_rarity,
        })
        try:
            self.persistent.record_episode(
                score=self._prev_score,
                primary_loadout=primary_loadout,
                killed_by_name=killed_by,
            )
            self.persistent.save()
        except OSError as e:
            print(f"[{self.name}] memory save failed: {e}")
        # Wipe the per-life scratchpad. Chat log stays — it's a global stream.
        self.episodic.clear()

        self.episode_reward = 0.0
        self.episode_len = 0
        self._prev_state = None
        self._prev_action = None
        self._prev_player_id = None
        self._prev_score = 0
        self._prev_hp = 1.0
        self._held_action = None
        self._repeat_left = 0
        # Drop the per-life HP attribution snapshot so the next life starts
        # clean (otherwise we'd credit a stale "HP went from 1.0 to 0.5"
        # against the new player on first tick).
        self._enemy_hp_snapshot.clear()
        self._my_petals_snapshot = []
        self.damage_dealt = 0.0
        # Drop the approach baseline so respawning at a far-away position
        # doesn't register as a huge "moved away" penalty next tick.
        self._prev_nearest_hostile_dist = None
        self.approach_reward_total = 0.0
        # Reset per-life extras counters for the next episode.
        self.pvp_kills_this_life = 0
        self.drops_picked_up_this_life = 0
        self._prev_nonempty_slots = 0

    def _decide_and_learn(self) -> tuple[float, float, int] | None:
        """Returns (ax, ay, flags) or None if we have nothing to do this tick."""
        cam = self._my_camera()
        player = self._my_player()

        # Death detection: we had a player last tick, now we don't.
        if player is None:
            if self._prev_player_id is not None:
                self._on_death(_ZERO_STATE)
            return None

        # New player entity (just respawned) — start a fresh episode without
        # crediting the spawn-XP grant.
        if self._prev_player_id != player["_id"]:
            if self._prev_player_id is not None:
                # We had an old player and now we have a new one — count the
                # transition as a death even if the server happened to ship
                # both updates in the same tick.
                self._on_death(_ZERO_STATE)
            self._prev_player_id = player["_id"]
            self._prev_score = int(player.get("score", 0))
            self._prev_hp = float(player.get("health_ratio", 1.0))

        my_team = cam.get("team", (0, 0)) if cam else (0, 0)
        hostile_feats = _hostile_features(self.entities, player, my_team)
        loadout_feats = _loadout_features(player)
        loadout_type_feats = _loadout_type_features(player)
        drop_feats = _drop_features(self.entities, player)
        loadout_burst_feats = _loadout_burst_features(player)
        # The peer-features slot is the path by which other bots' model
        # outputs reach our QNet input. Read happens *before* publish, so we
        # condition on whatever peers decided last tick — never on our own
        # current decision (no self-loops).
        peer_feats = _peer_features(self.agent, self.name, player)
        # Wall-distance sensors — 4 cardinal-direction rays to the
        # nearest wall AABB (or arena boundary), normalised to [0, 1].
        # Lets the policy learn "I'm cornered, back off" without
        # waiting to grind against geometry.
        if _WALL_RECTS or _WALL_WORLD_W > 0:
            wall_feats = wall_ray_features(
                float(player.get("x", 0.0)), float(player.get("y", 0.0)),
                _WALL_WORLD_W, _WALL_WORLD_H, _WALL_RECTS,
            )
        else:
            wall_feats = _ZERO_WALL_FEATS
        state = _build_state(
            player,
            hostile_feats,
            loadout_feats,
            peer_feats,
            loadout_type_feats,
            drop_feats,
            loadout_burst_feats,
            wall_feats,
        )

        cur_score = int(player.get("score", 0))
        cur_hp = float(player.get("health_ratio", 1.0))

        # Push the previous transition with the reward observed *now*.
        if self._prev_state is not None and self._prev_action is not None:
            d_score = max(0, cur_score - self._prev_score)  # ignore score drops on respawn
            d_hp = cur_hp - self._prev_hp
            reward = W_SCORE * d_score + W_HP * d_hp - IDLE_PENALTY
            # One-shot cost on the *action* that caused a delete this tick.
            # Credit-assigned to the delete action directly, so the QNet
            # learns "delete = expensive" without waiting for the empty-slot
            # penalty stream to flow through several future ticks.
            prev_kind, prev_p1, _ = decode_inventory_action(self._prev_action)
            if prev_kind == "delete":
                # Scale the cost by the rarity of what we just deleted, so
                # tossing a kUnique is far more painful than tossing a
                # kCommon. The rank-norm sits in `_prev_state` at the
                # loadout-feature column for slot prev_p1; recovering rank
                # is `norm * (_MAX_RARITY_RANK + 1) - 1`. norm=0 means the
                # slot was already empty — still pays the base cost so
                # spamming delete on empty slots stays discouraged.
                rarity_factor = 1.0
                if 0 <= prev_p1 < 16 and self._prev_state is not None:
                    norm = self._prev_state[LOADOUT_RANK_OFFSET + prev_p1]
                    if norm > 0.0:
                        rank = norm * (_MAX_RARITY_RANK + 1) - 1
                        rarity_factor = 1.0 + RARITY_PRESENT_SCALE * max(0.0, rank)
                reward -= W_DELETE_COST * rarity_factor
            # Engagement shaping #2: small per-tick bonus while *any* hostile
            # (mob or enemy player) is within engagement range. Continuous
            # gradient toward "seek a fight," even before any kill resolves.
            # Generalised from PvP-only because bots had no per-tick gradient
            # for being near mobs and were ignoring them.
            near_d2 = _nearest_hostile_dist_sq(self.entities, player, my_team)
            if near_d2 < PROXIMITY_RANGE * PROXIMITY_RANGE:
                reward += W_PROXIMITY
                self.engagement_ticks += 1
            # Engagement shaping #3: actual damage dealt, attributed via
            # *per-petal* collision against each enemy whose HP just dropped.
            # The world update we just parsed contains every visible petal
            # entity (Petal component, parent == our player ID, has its own
            # x/y/radius from the Physics component). For each enemy with an
            # HP drop, we check whether any of our petals overlaps it
            # (sum-of-radii squared compare). If yes, credit the drop to us.
            #
            # Why per-petal and not a single "in range of player" check:
            # petals orbit at ~50 from the player center and do the actual
            # damage on contact, so a player-centred radius either over-
            # credits (loose radius catches kills the petals couldn't have
            # made) or under-credits (tight radius misses petals at the far
            # edge of the orbit). Iterating actual petal positions is what
            # the server's collision system itself does.
            #
            # `pending_delete` entities are *included* on both sides:
            #   - dying enemies have HP=0 and pending_delete=1 on the same
            #     packet, then disappear the following tick. This frame is
            #     the kill blow and we want to credit it.
            #   - dying petals (depleted reload, hit something fragile) are
            #     in the dict for the same reason; if their last-known
            #     position overlapped the dying enemy, that's our credit.
            # Damage credit, split by victim type so player damage can pay
            # the W_PVP_BONUS multiplier — that's the only mechanism that
            # counts as PvP. No score-delta heuristic, no client-side
            # prediction: a hit is "PvP" iff a petal we own overlapped a
            # hostile Flower's body when its HP went down.
            mob_damage = 0.0
            player_damage = 0.0
            confirmed_pvp_kills = 0
            confirmed_mob_kills = 0
            my_player_id = player["_id"]
            my_petals: list[tuple[float, float, float]] = []
            for eid, ent in self.entities.items():
                if not has_component(ent, "Petal"):
                    continue
                if ent.get("parent") != my_player_id:
                    continue
                if "x" not in ent:
                    continue
                my_petals.append((
                    float(ent["x"]),
                    float(ent["y"]),
                    float(ent.get("radius", 10.0)),
                ))
            if my_petals:
                # Path 1: enemies still in our view that took damage. Check
                # current petal overlap against current enemy position.
                seen_eids: set[tuple[int, int]] = set()
                for eid, ent in self.entities.items():
                    if eid == my_player_id:
                        continue
                    is_flower = has_component(ent, "Flower")
                    is_mob = has_component(ent, "Mob")
                    if not (is_flower or is_mob):
                        continue
                    if "x" not in ent:
                        continue
                    if ent.get("team", (0, 0)) == my_team:
                        continue
                    seen_eids.add(eid)
                    cur_e_hp = float(ent.get("health_ratio", 1.0))
                    snap = self._enemy_hp_snapshot.get(eid)
                    prev_e_hp = snap[0] if snap is not None else cur_e_hp
                    drop = prev_e_hp - cur_e_hp
                    if drop <= 0.001:
                        continue
                    ex = float(ent["x"])
                    ey = float(ent["y"])
                    er = float(ent.get("radius", 25.0))
                    for px_, py_, pr in my_petals:
                        rsum = pr + er
                        dx = ex - px_
                        dy = ey - py_
                        if dx * dx + dy * dy < rsum * rsum:
                            if is_flower:
                                player_damage += drop
                            else:
                                mob_damage += drop
                            break

                # Path 2: enemies that vanished from our view since last
                # tick. Between two control-loop ticks the server can ship
                # both the kill packet *and* the subsequent deletion, so by
                # the time we look at `entities` the dying entity is gone.
                # We use the snapshot's last-known position, petal positions,
                # *and* is_flower flag — all from the same tick — to credit
                # the kill blow correctly and tag it as PvP if the victim
                # was a player.
                snap_petals = self._my_petals_snapshot
                for eid in list(self._enemy_hp_snapshot.keys()):
                    if eid in seen_eids:
                        continue
                    snap_hp, sx, sy, sr, snap_is_flower = self._enemy_hp_snapshot[eid]
                    if snap_hp <= 0.001:
                        del self._enemy_hp_snapshot[eid]
                        continue
                    for px_, py_, pr in snap_petals:
                        rsum = pr + sr
                        dx = sx - px_
                        dy = sy - py_
                        if dx * dx + dy * dy < rsum * rsum:
                            if snap_is_flower:
                                player_damage += snap_hp
                                confirmed_pvp_kills += 1
                            else:
                                mob_damage += snap_hp
                                confirmed_mob_kills += 1
                            break
                    del self._enemy_hp_snapshot[eid]

            damage_credited = mob_damage + player_damage
            if damage_credited > 0.0:
                # Mobs pay base damage; players pay base + PvP bonus. The
                # only thing distinguishing PvP from PvE in the reward is
                # this multiplier on petal-confirmed Flower damage.
                reward += W_DAMAGE * mob_damage
                reward += W_DAMAGE * (1.0 + W_PVP_BONUS) * player_damage
                # Additional petal-damage bonus, applied uniformly to both
                # mob and player petal damage. Every credited damage point
                # is already evidence of petal-on-flesh contact (the loops
                # above only credit when a snapshot petal overlapped the
                # snapshot enemy), so this just scales that signal up.
                reward += W_PETAL_DAMAGE_BONUS * damage_credited
                self.damage_dealt += damage_credited
                self.damage_dealt_lifetime += damage_credited
            # Flat kill-event bonuses on top of the per-HP-ratio damage.
            # Both fire only on a *confirmed* petal-overlap-on-disappearance
            # — same evidence path as the damage credit; no score-delta or
            # other heuristic. The asymmetry encodes "killing another bot
            # is worth way more than killing a mob."
            if confirmed_mob_kills:
                reward += W_MOB_KILL_BONUS * confirmed_mob_kills
            if confirmed_pvp_kills:
                reward += W_PLAYER_KILL_BONUS * confirmed_pvp_kills
            self.pvp_kills += confirmed_pvp_kills
            self.pvp_kills_this_life += confirmed_pvp_kills
            # Approach shaping: dense per-tick gradient toward "any hostile."
            # Computes change in the nearest-hostile distance vs. last tick;
            # capped so a target swap (kill, target despawn, or a closer
            # entity entering view) doesn't dominate the term. The cap is
            # symmetric — same magnitude either way — so the policy can't
            # game it by intentionally swapping targets.
            cur_d2 = _nearest_hostile_dist_sq(self.entities, player, my_team)
            if cur_d2 < math.inf:
                cur_d = math.sqrt(cur_d2)
                if self._prev_nearest_hostile_dist is not None:
                    closing = self._prev_nearest_hostile_dist - cur_d
                    if closing > APPROACH_CAP:
                        closing = APPROACH_CAP
                    elif closing < -APPROACH_CAP:
                        closing = -APPROACH_CAP
                    approach_r = W_APPROACH * (closing / OBS_SCALE)
                    reward += approach_r
                    self.approach_reward_total += approach_r
                self._prev_nearest_hostile_dist = cur_d
            else:
                # No hostile in view — clear the baseline so we don't apply
                # a spurious closing reward when one re-enters.
                self._prev_nearest_hostile_dist = None
            # Petal-having reward / empty-slot penalty. Two-sided gradient
            # on the loadout: every non-empty primary slot pays the full
            # rarity-scaled per-tick bonus, every kNone primary slot pays
            # a per-tick penalty, and every non-empty *storage* slot pays
            # W_PETAL_STORAGE_RATIO × the primary rate. The storage half
            # is what gives the policy a gradient to walk over a drop even
            # when its primary row is already full — without it, an Epic
            # petal on the ground next to a full-primary bot was worth
            # zero per-tick and the model never bothered with it.
            loadout_ids = player.get("loadout_ids")
            loadout_count = int(player.get("loadout_count", 0))
            if loadout_ids and loadout_count > 0:
                primary_end = min(loadout_count, len(loadout_ids))
                total_end = len(loadout_ids)
                # Drop-pickup detector: pickup automatically fills the
                # first empty loadout slot (Server/Process/Collision.cc),
                # so count(non-kNone) increases by 1 per pickup. Swaps
                # don't change the count, so they don't false-positive.
                # Credited per-life and pushed into the rolling extras
                # window via record_episode_extras() on death — that's
                # the signal run.py uses to maintain .best.drops.
                cur_nonempty = sum(1 for pid in loadout_ids if int(pid) != PETAL_NONE)
                if cur_nonempty > self._prev_nonempty_slots:
                    self.drops_picked_up_this_life += cur_nonempty - self._prev_nonempty_slots
                self._prev_nonempty_slots = cur_nonempty
                # Per-slot rarity-scaled bonus. A slot with kCommon pays
                # 1.0× W_PETAL_PRESENT; a slot with kUnique pays
                # (1 + 0.5 × 6) = 4.0×. This gives a direct, immediate
                # gradient that "rarer = more valuable to hold," which the
                # flat sum couldn't express.
                slot_value = 0.0
                empty_primary = 0
                for i in range(primary_end):
                    pid = int(loadout_ids[i])
                    if pid == PETAL_NONE:
                        empty_primary += 1
                        continue
                    rank = petal_rank(pid)
                    if rank < 0:
                        slot_value += 1.0
                    else:
                        slot_value += 1.0 + RARITY_PRESENT_SCALE * rank
                # Storage slots — same rarity scaling but discounted by
                # W_PETAL_STORAGE_RATIO so primary stays the preferred
                # destination for new pickups. Empty storage slots are
                # *not* penalised (only primary emptiness costs).
                storage_value = 0.0
                for i in range(primary_end, total_end):
                    pid = int(loadout_ids[i])
                    if pid == PETAL_NONE:
                        continue
                    rank = petal_rank(pid)
                    if rank < 0:
                        storage_value += 1.0
                    else:
                        storage_value += 1.0 + RARITY_PRESENT_SCALE * rank
                if slot_value > 0.0:
                    reward += W_PETAL_PRESENT * slot_value
                if storage_value > 0.0:
                    reward += W_PETAL_PRESENT * W_PETAL_STORAGE_RATIO * storage_value
                if empty_primary > 0:
                    reward -= W_EMPTY_PRIMARY_PENALTY * empty_primary
            self.episode_reward += reward
            self.agent.push(self._prev_state, self._prev_action, reward, state, False)
            # Episodic memory: log positive score deltas so chat triggers can
            # detect "big kill" events. Mob kills are typically +1..+5; a
            # sudden +20+ usually means we killed (or crit-hit) a player.
            if d_score > 0:
                self.episodic.note_score_delta(d_score)

        # Refresh the hostile snapshot for next tick's damage attribution.
        # Stores (hp, x, y, radius) per entity. Position is needed because
        # an entity that dies between control ticks is already gone from
        # `self.entities` by the time we look — we need its last known
        # position to test petal overlap. Includes mobs *and* enemy players
        # *and* pending_delete entities for the same reason.
        for eid, ent in self.entities.items():
            is_flower = has_component(ent, "Flower")
            is_mob = has_component(ent, "Mob")
            if not (is_flower or is_mob):
                continue
            if "x" not in ent:
                continue
            if ent.get("team", (0, 0)) == my_team:
                continue
            self._enemy_hp_snapshot[eid] = (
                float(ent.get("health_ratio", 1.0)),
                float(ent["x"]),
                float(ent["y"]),
                float(ent.get("radius", 25.0)),
                bool(is_flower),
            )
        # Mirror the petal positions taken this tick. Used by the
        # disappearance-credit path so the overlap check uses petals from
        # the same moment as the snapshotted enemy positions.
        my_player_id_for_snap = player["_id"]
        self._my_petals_snapshot = [
            (float(ent["x"]), float(ent["y"]), float(ent.get("radius", 10.0)))
            for ent in self.entities.values()
            if has_component(ent, "Petal")
            and ent.get("parent") == my_player_id_for_snap
            and "x" in ent
        ]

        # Track the current primary loadout so we can credit best_loadout
        # on death. Snap-shotted every tick so the latest equipment sticks
        # even if we die mid-swap.
        ids = player.get("loadout_ids")
        lc = int(player.get("loadout_count", 0))
        if ids and lc > 0:
            self.episodic.last_primary_loadout = [int(x) for x in ids[:lc]]

        # Action repeat applies only to movement actions — inventory actions
        # are one-shot so the model can't accidentally hold a delete for
        # several ticks and trash multiple petals.
        if self._held_action is None or self._repeat_left <= 0:
            self._held_action = self.agent.act(state)
            self._repeat_left = (
                self.action_repeat
                if is_movement_action(self._held_action)
                else 1  # one-shot for swap / delete
            )
        action = self._held_action
        self._repeat_left -= 1

        # Translate the chosen action. Inventory actions still emit a
        # movement input (stay-still + attack) so we don't drop input frames
        # from the server's POV; the actual swap/delete packet is queued for
        # the control loop to send right after.
        if is_movement_action(action):
            self._pending_inventory_action = None
            ax, ay, flags = action_to_input(action, MOVE_MAG, INPUT_ATTACK, INPUT_DEFEND)
        else:
            self._pending_inventory_action = decode_inventory_action(action)
            ax, ay, flags = 0.0, 0.0, INPUT_ATTACK

        # Publish our latest decision to the shared agent blackboard so other
        # bots can splice it into their next observation. This is the path
        # that lets one bot's model output flow into another bot's input.
        # `time.time()` lets readers filter by staleness without us having to
        # tell them about deaths.
        import time as _t
        self.agent.publish(self.name, {
            "x": float(player.get("x", 0.0)),
            "y": float(player.get("y", 0.0)),
            "hp": cur_hp,
            "action": int(action),
            "time": _t.time(),
        })

        self._prev_state = state
        self._prev_action = action
        self._prev_score = cur_score
        self._prev_hp = cur_hp
        self.episode_len += 1
        return ax, ay, flags

    # -- main loop ----------------------------------------------------------

    async def run_once(self) -> None:
        async with websockets.connect(self.url, max_size=2 * 1024 * 1024) as ws:
            await self._send_verify(ws)
            await self._send_spawn(ws)
            # The initial spawn counts as our one allowed spawn for the
            # current round — don't let control_loop double-spawn before
            # the next kRoundEnd re-arms us.
            self._can_respawn = False
            stop = asyncio.Event()
            # Sync-mode rendezvous: every C_CLIENT_UPDATE flips this
            # event so the control loop can `await update_event.wait()`
            # in place of `asyncio.sleep(period)`. Lockstep cadence is
            # then: server tick → broadcast update → bot receives →
            # event set → control_loop wakes → decide → send input +
            # S_STEP → server collects steps, ticks again. The event is
            # cleared inside the control loop after each consumption.
            update_event = asyncio.Event()
            # Bootstrap S_STEP for sync mode. The server doesn't tick
            # until every verified client has stepped at least once, so
            # without this kick-off the bot would dead-lock waiting for
            # an update that never comes.
            if self.sync_mode:
                try:
                    await self._send_step(ws)
                except websockets.ConnectionClosed:
                    return

            async def recv_loop():
                try:
                    async for msg in ws:
                        if not isinstance(msg, (bytes, bytearray)) or not msg:
                            continue
                        op = msg[0]
                        if op == C_CLIENT_UPDATE:
                            try:
                                cam_id = parse_client_update(
                                    msg, self.entities, self.arena
                                )
                                if cam_id is not None:
                                    self.camera_id = cam_id
                            except (IndexError, ValueError) as e:
                                print(f"[{self.name}] parse error: {e}")
                                self.entities.clear()
                                self.camera_id = None
                            update_event.set()
                        elif op == C_OUTDATED:
                            print(f"[{self.name}] server says client is outdated; bump VERSION_HASH")
                            stop.set()
                            return
                        elif op == C_ROUND_END:
                            parsed = parse_round_end(msg)
                            if parsed is not None:
                                winner_name, winner_score = parsed
                                self.rounds_seen += 1
                                if winner_name == self.name:
                                    self._round_win_pending = W_ROUND_WIN
                                    self.rounds_won += 1
                                # Re-arm respawn — every bot waits for this
                                # signal between rounds so they all spawn
                                # together at round start.
                                self._can_respawn = True
                        elif op == C_CHAT:
                            parsed = parse_chat_packet(msg)
                            if parsed is not None:
                                sender, color, text = parsed
                                self.episodic.note_chat(sender, color, text)
                                # Try structured-data chat first. Anything
                                # else is treated as plain human text. We
                                # skip self-loops on either path so the log
                                # stays readable.
                                data = decode_data_chat(text)
                                if data is not None:
                                    kind = data.get("kind", "?")
                                    by_sender = self.episodic.peer_state.setdefault(sender, {})
                                    by_sender[kind] = (asyncio.get_event_loop().time(), data)
                                    if sender != self.name:
                                        # Render the structured payload compactly.
                                        details = ", ".join(
                                            f"{k}={v}" for k, v in data.items() if k != "kind"
                                        )
                                        # print(f"  <data> [{sender}] {kind}: {details}", flush=True)
                                # elif sender != self.name:
                                #     print(f"  <chat> [{sender}] {text}", flush=True)
                finally:
                    stop.set()

            async def control_loop():
                period = 1.0 / self.control_hz
                last_spawn = 0.0
                while not stop.is_set():
                    if self.sync_mode:
                        # Lockstep: wait until the next world update lands.
                        # Clear immediately after awaiting so we won't
                        # re-fire if multiple updates arrived between
                        # iterations (shouldn't happen in sync mode but
                        # is harmless to guard against).
                        try:
                            await update_event.wait()
                        except asyncio.CancelledError:
                            return
                        update_event.clear()
                    else:
                        await asyncio.sleep(period)
                    # Mark when this tick's work starts so we can measure
                    # whether the loop is keeping up with `control_hz`.
                    # The asyncio scheduler clamps `sleep` to "at least
                    # period", so wall_now − tick_start gives us the work
                    # we're doing on top of the base sleep — if it
                    # exceeds period, the next tick fires immediately and
                    # the realised env_step rate falls below configured.
                    tick_start = asyncio.get_event_loop().time()

                    # In sync mode we MUST send S_STEP every iteration —
                    # otherwise a dead bot stops responding to the barrier
                    # and the whole server hangs. This nested helper does
                    # nothing in wall-clock mode and one packet in sync mode;
                    # called from each `continue` path below.
                    async def _maybe_step():
                        if self.sync_mode:
                            try:
                                await self._send_step(ws)
                            except websockets.ConnectionClosed:
                                pass

                    cam = self._my_camera()
                    if cam is None:
                        await _maybe_step()
                        self._record_tick_work(
                            asyncio.get_event_loop().time() - tick_start, period
                        )
                        continue
                    player = self._my_player()
                    if player is None:
                        # Dead — credit the death once.
                        if self._prev_player_id is not None:
                            self._on_death(_ZERO_STATE)
                        now = asyncio.get_event_loop().time()
                        # Wave-system respawn gate: only allowed once per
                        # round. _can_respawn is flipped True on each
                        # kRoundEnd, then back to False here when we
                        # actually fire the spawn — so a bot can't quietly
                        # double-spawn within a round.
                        # `respawn_immediately` is the --fast-start override:
                        # ignore the round gate and re-spawn as soon as we
                        # can. Crucial for early DQN training, where episode
                        # turnover is the dominant learning signal.
                        gate_ok = self.respawn_immediately or self._can_respawn
                        if gate_ok and now - last_spawn > 1.0:
                            try:
                                await self._send_spawn(ws)
                            except websockets.ConnectionClosed:
                                return
                            last_spawn = now
                            if not self.respawn_immediately:
                                self._can_respawn = False
                        await _maybe_step()
                        self._record_tick_work(
                            asyncio.get_event_loop().time() - tick_start, period
                        )
                        continue
                    just_respawned = self._prev_player_id is None
                    decision = self._decide_and_learn()
                    if decision is None:
                        await _maybe_step()
                        self._record_tick_work(
                            asyncio.get_event_loop().time() - tick_start, period
                        )
                        continue
                    ax, ay, flags = decision
                    try:
                        await self._send_input(ws, ax, ay, flags)
                    except websockets.ConnectionClosed:
                        return

                    # Chat triggers. Mix of human-readable and structured:
                    #  - Respawn quip is text (it's tagged for human
                    #    debug-watching, summarises persistent stats).
                    #  - Kill / help / position are *binary* MSG_* payloads
                    #    so peers can act on them mechanically rather than
                    #    parsing English.
                    now_t = asyncio.get_event_loop().time()
                    if just_respawned and self.persistent.episodes > 0:
                        msg = self._respawn_quip()
                        if msg:
                            await self._send_chat(ws, msg)
                    elif self.episodic.score_deltas:
                        recent_t, recent_d = self.episodic.score_deltas[-1]
                        if recent_d >= 20 and now_t - recent_t < 0.3:
                            await self._send_chat(
                                ws,
                                encode_kill(int(recent_d), float(player["x"]), float(player["y"])),
                                kind="kill",
                            )
                    if (not self.episodic.shouted_low_hp
                            and player.get("health_ratio", 1.0) < 0.30):
                        self.episodic.shouted_low_hp = True
                        await self._send_chat(
                            ws,
                            encode_help(
                                float(player["x"]),
                                float(player["y"]),
                                float(player.get("health_ratio", 0.0)),
                            ),
                            kind="help",
                        )
                    # Periodic position broadcast — every ~2 s wall-clock so
                    # peers maintain a coarse mental map of the swarm. Throttled
                    # per-kind via EpisodicMemory.last_data_sent so it doesn't
                    # crowd out kill/help when those need to fire.
                    last_pos_t = self.episodic.last_data_sent.get("position", 0.0)
                    if now_t - last_pos_t >= 2.0:
                        await self._send_chat(
                            ws,
                            encode_position(
                                float(player["x"]),
                                float(player["y"]),
                                float(player.get("health_ratio", 1.0)),
                            ),
                            kind="position",
                        )

                    # Inventory action: when the model picks a swap or
                    # delete, `_decide_and_learn` queues it on
                    # `_pending_inventory_action`. We send the matching
                    # packet *after* the input packet so the bot's tick has
                    # both a movement input and the inventory mutation.
                    pending = self._pending_inventory_action
                    if pending is not None:
                        kind, p1, p2 = pending
                        try:
                            if kind == "swap":
                                await self._send_petal_swap(ws, p1, p2)
                                self.swaps_sent += 1
                            elif kind == "delete":
                                await self._send_petal_delete(ws, p1)
                                self.deletes_sent += 1
                        except websockets.ConnectionClosed:
                            return
                        self._pending_inventory_action = None

                    # Sync-mode pacer: tell the server we're done thinking
                    # and ready for the next tick. Sent *after* the input
                    # so the server processes our action this tick rather
                    # than the next one. In wall-clock mode this is
                    # skipped (the server's auto-timer is driving).
                    if self.sync_mode:
                        try:
                            await self._send_step(ws)
                        except websockets.ConnectionClosed:
                            return

                    # End of the busy iteration: record how long the work
                    # actually took. Includes state-build, agent.act,
                    # potential train_step, and any send awaits that
                    # resolved promptly. Long awaits (e.g. blocked on a
                    # full TCP buffer) will show up here as overruns.
                    self._record_tick_work(
                        asyncio.get_event_loop().time() - tick_start, period
                    )

            await asyncio.gather(recv_loop(), control_loop())

    async def run_forever(self, reconnect_delay: float = 2.0) -> None:
        while True:
            try:
                await self.run_once()
            except (OSError, websockets.WebSocketException) as e:
                print(f"[{self.name}] connection error: {e}")
            except asyncio.CancelledError:
                raise
            except Exception as e:  # noqa: BLE001
                print(f"[{self.name}] unexpected: {e!r}")
            # Disconnect counts as a death so we don't keep stale prev-state
            # bridging a connection break.
            if self._prev_player_id is not None:
                self._on_death(_ZERO_STATE)
            self.entities.clear()
            self.camera_id = None
            await asyncio.sleep(reconnect_delay)


# Back-compat alias so old scripts that import Bot still work.
Bot = LearningBot
