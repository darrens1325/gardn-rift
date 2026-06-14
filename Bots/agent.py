"""Shared DQN agent for the gardn bots.

One QNet is shared across every bot in the process. Bots push transitions into
a single replay buffer and consult the same network for actions, so N bots in
parallel give us roughly N× the experience per wall-clock second.

Action layout (18):
    0..8:  {stay, N, NE, E, SE, S, SW, W, NW} with the attack input held.
    9..17: same nine moves but with the defend input held instead.

Defend in gardn pulls petals into a tight cluster around the player — high
single-target DPS at the cost of orbital coverage. Putting attack/defend in
the action space lets the agent learn when each mode pays off rather than
being told.

State layout (13):
    [hp,                                                 # self
     dx0/SCALE, dy0/SCALE, is_player0, hp0,              # nearest hostile
     dx1/SCALE, dy1/SCALE, is_player1, hp1,
     dx2/SCALE, dy2/SCALE, is_player2, hp2]

Empty slots are zero-padded. SCALE keeps inputs in roughly [-1, 1].
"""

from __future__ import annotations

import math
import os
import random
import threading
from collections import deque

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim

# Observation layout. The original 13-dim "world view" still drives the bulk
# of the policy; the comm slots that follow give the bot direct access to
# what other bots in the swarm have just decided to do.
BASE_STATE_DIM = 13      # self HP + 3 nearest hostiles × 4 features
K_PEERS = 2              # how many peer messages we splice in
COMM_PER_PEER = 6        # rel_dx, rel_dy, hp, peer_vx, peer_vy, peer_attacking
COMM_DIM = K_PEERS * COMM_PER_PEER
# One float per loadout slot (16 = 2 * MAX_SLOT_COUNT), encoding the petal's
# rarity rank shifted so empty == 0 and higher rarity → higher value. Lets
# the policy actually distinguish "this slot has a basic" from "this slot
# has a legendary," so the swap/delete actions are no longer chosen
# blindly.
LOADOUT_FEAT_DIM = 16
# Parallel slot vector encoding the petal's *role* (damage / tank / heal /
# poison / utility) — see protocol.py:PETAL_TYPE. With both rank and type
# the network can answer "is this a healer or a damage petal of common
# rarity?" rather than just "is this slot full of something common?"
LOADOUT_TYPE_DIM = 16
# Per-slot effective burst (damage × count, normalised by 150). Lets the
# network see absolute power: a kEpicHeavy (70) beats a kEpicLight (35)
# even though both are rank=Epic + type=damage. Pairs with rank+type so
# the model has a complete picture of each slot's value.
LOADOUT_BURST_DIM = 16
# Up to K_DROPS nearest ground drops, encoded as
#   (rel_dx_norm, rel_dy_norm, drop_rank_norm, drop_type_norm).
# Lets the bot navigate toward useful drops (e.g. a nearby epic petal is
# worth detouring for). Empty slots are zero-padded.
K_DROPS = 3
DROP_FEAT_PER_SLOT = 4
DROP_FEAT_DIM = K_DROPS * DROP_FEAT_PER_SLOT
# 4 cardinal-direction wall-distance sensors (N/E/S/W), each normalised
# to [0, 1] where 1 means "≥ WALL_RAY_CAP units of clear space". Read at
# tick time from the static Tiled map (see Bots/wall_map.py). Lets the
# policy learn wall avoidance — without these, the bot only sees enemies
# and inventory and just grinds against the geometry.
from wall_map import (  # noqa: E402  (kept inline so STATE_DIM follows)
    MINIMAP_FEAT_DIM,
    WALL_FEAT_DIM,
    WARP_FEAT_DIM,
)
# State layout is **append-only** with respect to previous versions so
# pad_load_state_dict can preserve trained weights from older checkpoints.
# Append, never reorder. Boundaries:
#   [HP] [hostile×12] [loadout_rank×16] [peer_comm×12]            |  ← old 41
#       [loadout_type×16] [drops×12]                              |  ← old 69
#       [loadout_burst×16]                                        |  ← +16 = 85
#       [wall_rays×4]                                             |  ← +4 = 89
#       [warps×8]                                                 |  ← +8 = 97
#       [minimap×3]                                               |  ← +3 = 100
# The trailing warp/minimap columns are how the bot "sees" portals
# and where it sits on the global map — features parallel to the
# minimap a human player reads.
STATE_DIM = (
    BASE_STATE_DIM            # 13
    + LOADOUT_FEAT_DIM        # 16
    + COMM_DIM                # 12
    + LOADOUT_TYPE_DIM        # 16
    + DROP_FEAT_DIM           # 12
    + LOADOUT_BURST_DIM       # 16
    + WALL_FEAT_DIM           # 4
    + WARP_FEAT_DIM           # 8
    + MINIMAP_FEAT_DIM        # 3
)                              # = 100
# Action space layout (flat):
#   0..NUM_MOVEMENT_ACTIONS-1            movement (9 directions × 2 modes)
#   NUM_MOVEMENT_ACTIONS..+NUM_SWAP-1    swap loadout_ids[i] ↔ loadout_ids[i+8]
#                                         for i in 0..7 (slot-pair swaps)
#   ...+NUM_DELETE-1                     delete loadout_ids[i] for i in 0..15
#
# Inventory actions are one-shot (no action-repeat) so the model can't
# accidentally hold a delete for several ticks and trash multiple petals.
NUM_MOVEMENT_ACTIONS = 18
NUM_SWAP_ACTIONS = 8
NUM_DELETE_ACTIONS = 16
NUM_INVENTORY_ACTIONS = NUM_SWAP_ACTIONS + NUM_DELETE_ACTIONS
NUM_ACTIONS = NUM_MOVEMENT_ACTIONS + NUM_INVENTORY_ACTIONS  # 42
NUM_DIRECTIONS = 9       # stay + 8 compass directions
OBS_SCALE = 1500.0       # rough "in combat" radius

# Max age in wall-clock seconds for a peer message to count toward the
# observation. Older entries are treated as "no peer" and zero-padded.
PEER_MESSAGE_TTL = 5.0

# (vx, vy) for each direction. Index 0 is stay.
_DIRECTIONS: list[tuple[float, float]] = [(0.0, 0.0)]
for k in range(8):
    a = k * math.pi / 4.0
    _DIRECTIONS.append((math.cos(a), math.sin(a)))


def action_to_input(action: int, move_mag: float, attack_flag: int, defend_flag: int) -> tuple[float, float, int]:
    """Map a movement action index (0..NUM_MOVEMENT_ACTIONS-1) to
    (ax, ay, input_flags) for the wire input. Inventory actions (>=
    NUM_MOVEMENT_ACTIONS) should be routed through `decode_inventory_action`
    instead — calling this on one would silently produce a bogus direction."""
    direction = action % NUM_DIRECTIONS
    use_defend = action >= NUM_DIRECTIONS and action < NUM_MOVEMENT_ACTIONS
    vx, vy = _DIRECTIONS[direction]
    flags = defend_flag if use_defend else attack_flag
    return vx * move_mag, vy * move_mag, flags


def is_movement_action(action: int) -> bool:
    return action < NUM_MOVEMENT_ACTIONS


def decode_inventory_action(action: int) -> tuple[str, int, int]:
    """Translate an inventory action index into a (kind, pos1, pos2) triple.
        kind == "swap":   server gets kPetalSwap with pos1 and pos2.
        kind == "delete": server gets kPetalDelete with pos1; pos2 == -1.
    Returns ("noop", -1, -1) for any movement-range index, so callers can
    safely pass any action without first classifying it."""
    if action < NUM_MOVEMENT_ACTIONS:
        return ("noop", -1, -1)
    rel = action - NUM_MOVEMENT_ACTIONS
    if rel < NUM_SWAP_ACTIONS:
        # Swap loadout_ids[i] with loadout_ids[i + 8]. The "+8" lands in the
        # storage region for max-level players (loadout_count=8) and in a
        # mixed primary/storage region at lower levels — the server's bounds
        # check (`pos < MAX_SLOT_COUNT + loadout_count`) drops out-of-range
        # ones silently, so the policy can pick any of the 8 freely.
        i = rel
        return ("swap", i, i + 8)
    rel -= NUM_SWAP_ACTIONS
    if rel < NUM_DELETE_ACTIONS:
        return ("delete", rel, -1)
    return ("noop", -1, -1)


def _resolve_device(name: str) -> torch.device:
    """Resolve a --device string. "auto" prefers the Apple GPU (mps),
    then cuda, then cpu — training on MPS halves the gradient-step cost
    on M-series machines *and* frees the CPU torch threads for the
    asyncio control loops, which is where the CPU-overrun warnings were
    coming from."""
    if name == "auto":
        if torch.backends.mps.is_available():
            return torch.device("mps")
        if torch.cuda.is_available():
            return torch.device("cuda")
        return torch.device("cpu")
    return torch.device(name)


def _pad_load_state_dict(module: nn.Module, source: dict) -> dict:
    """Load a state-dict whose tensors may be smaller than the target's,
    copying overlapping slices and leaving the rest of the target at its
    random initialisation. Used to keep `model.pt` valuable across edits
    that grow the action space (final-layer out_features) or the state
    vector (input-layer in_features) — old weights for the surviving
    indices are preserved, new positions train from scratch.

    Returns a tiny report so the caller can log what was padded vs copied
    cleanly vs skipped (e.g., due to dim count mismatch)."""
    own = module.state_dict()
    padded: list[str] = []
    skipped: list[str] = []
    for key, src_tensor in source.items():
        if key not in own:
            continue
        dst_tensor = own[key]
        if dst_tensor.shape == src_tensor.shape:
            own[key] = src_tensor.to(dst_tensor.device, dst_tensor.dtype)
            continue
        if dst_tensor.dim() != src_tensor.dim():
            skipped.append(key)
            continue
        # Pad: copy the overlapping prefix in every dimension, leave any
        # extra positions in `dst_tensor` at their random init. Works for
        # both growth (dst > src) and shrinkage (src > dst).
        new_t = dst_tensor.clone()
        slc = tuple(slice(0, min(d, s)) for d, s in zip(dst_tensor.shape, src_tensor.shape))
        new_t[slc] = src_tensor[slc].to(dst_tensor.device, dst_tensor.dtype)
        own[key] = new_t
        padded.append(key)
    module.load_state_dict(own)
    return {"padded": padded, "skipped": skipped}


class QNet(nn.Module):
    def __init__(self) -> None:
        super().__init__()
        # Widened from 128/128/256/128 → 512/512/1024/512 (~100K → ~1.4M
        # parameters, ~14×). The narrower net plateaued at pvp_kills ≈
        # 0.08/episode at 150M env_steps and couldn't translate the
        # richer reward signal (camp penalty, weak/camper-kill bonuses,
        # player-specific proximity/approach) into behavior changes.
        # Old checkpoints still load via `_pad_load_state_dict` — the
        # legacy 128 hidden units occupy the first 128 positions of each
        # new wider layer, and the remaining positions train from
        # random init.
        self.net = nn.Sequential(
            nn.Linear(STATE_DIM, 512),
            nn.ReLU(),
            nn.Linear(512, 512),
            nn.ReLU(),
            nn.Linear(512, 1024),
            nn.ReLU(),
            nn.Linear(1024, 512),
            nn.ReLU(),
            nn.Linear(512, NUM_ACTIONS),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


class DQNAgent:
    """Vanilla DQN with a target network. Single-process, no multiprocessing."""

    def __init__(
        self,
        lr: float = 5e-4,
        gamma: float = 0.97,
        buffer_size: int = 100_000,
        batch_size: int = 128,
        eps_start: float = 1.0,
        eps_end: float = 0.05,
        eps_decay_steps: int = 30_000,
        target_sync_steps: int = 1_000,
        train_every: int = 4,
        warmup: int = 2_000,
        device: str = "auto",
        checkpoint_path: str | None = None,
        checkpoint_every_train_steps: int = 1_000,
    ) -> None:
        self.device = _resolve_device(device)
        # `q` is the live, training-mutated network — only the trainer
        # thread writes to it. `q_inference` is a periodically-synced copy
        # used by `act()` on the asyncio thread. Splitting them lets the
        # control loop's forward passes proceed without contending on the
        # weights the trainer is updating; the only point of contention
        # is the brief sync that copies state_dict() between them.
        #
        # `q_inference` is pinned to CPU regardless of the training
        # device: act() is a batch-1 forward, where MPS/CUDA dispatch
        # overhead exceeds the matmul itself, and a CPU-resident copy
        # means the asyncio thread never has to queue work on the
        # accelerator the trainer is saturating. load_state_dict copies
        # across devices, so the periodic q → q_inference sync works
        # unchanged.
        self.q = QNet().to(self.device)
        self.q_inference = QNet()  # CPU on purpose — see above
        self.q_inference.load_state_dict(self.q.state_dict())
        self.q_inference.eval()
        self.target = QNet().to(self.device)
        self.target.load_state_dict(self.q.state_dict())
        self.target.eval()
        self.optimizer = optim.Adam(self.q.parameters(), lr=lr)

        self.gamma = gamma
        # Replay buffer: preallocated numpy ring instead of a deque of
        # Python tuples. Three reasons, all measured:
        #   - random.sample on a 100k deque costs ~0.34 ms *under the
        #     buffer lock* (deque indexing is O(n)); fancy-indexing the
        #     numpy arrays is microseconds.
        #   - torch.as_tensor on nested Python lists held the GIL for
        #     ~1.5 ms per train step, stalling every bot's control loop
        #     even though the trainer runs on its own thread.
        #     torch.from_numpy on a contiguous float32 array is zero-copy.
        #   - 100k transitions as Python float lists is ~330 MB of small
        #     objects; the same data here is ~80 MB flat and invisible
        #     to the garbage collector.
        self.buffer_size = int(buffer_size)
        self._buf_s = np.zeros((self.buffer_size, STATE_DIM), dtype=np.float32)
        self._buf_a = np.zeros(self.buffer_size, dtype=np.int64)
        self._buf_r = np.zeros(self.buffer_size, dtype=np.float32)
        self._buf_s2 = np.zeros((self.buffer_size, STATE_DIM), dtype=np.float32)
        self._buf_d = np.zeros(self.buffer_size, dtype=np.float32)
        self._buf_pos = 0    # next write index
        self._buf_len = 0    # filled entries (saturates at buffer_size)
        self.batch_size = batch_size
        self.eps_start = eps_start
        self.eps_end = eps_end
        self.eps_decay_steps = eps_decay_steps
        self.target_sync_steps = target_sync_steps
        self.train_every = train_every
        self.warmup = warmup
        self.checkpoint_every_train_steps = checkpoint_every_train_steps

        self.env_steps = 0       # transitions pushed
        self.train_steps = 0     # gradient updates performed
        self.last_loss = 0.0

        # Thread plumbing for offloaded training. PyTorch ops release the
        # GIL during their C++ kernels (matmul / backward / Adam.step), so
        # a background trainer *thread* really does run in parallel with
        # the asyncio event loop on a separate core during compute. The
        # only Python-bound contention is buffer sampling and the brief
        # inference-net sync — both held under tight locks.
        self._buffer_lock = threading.Lock()
        self._sync_lock = threading.Lock()
        self._stop_event = threading.Event()
        self._trainer_thread: threading.Thread | None = None
        # How often (in train_steps) to copy q → q_inference. Inference
        # weights lag by up to this many gradient updates, which DQN
        # tolerates fine — the policy doesn't change radically per step.
        self._inference_sync_every = 10

        # Peer-sync: in multi-worker training, periodically average the
        # online net's weights with sibling workers' on-disk checkpoints.
        # Disabled by default (provider=None) — run.py wires this up when
        # `--workers > 1`. Implements federated-averaging-style sync so
        # all workers slowly converge on a shared model rather than each
        # one wasting its compute on an independent racing trajectory.
        # See `merge_peer_weights` for the actual merge implementation.
        self._peer_sync_paths_provider = None
        self._peer_sync_every_train_steps: int = 0
        self._peer_sync_my_weight: float = 0.5
        self._peer_sync_log: bool = True

        # Rolling window of recent finished-episode results across the entire
        # swarm. Bots call `record_episode()` on death. The mean over this
        # window is the actual learning curve — `last_episode_*` on a single
        # bot is too noisy because every death overwrites it.
        self._episode_scores: deque[int] = deque(maxlen=200)
        self._episode_rewards: deque[float] = deque(maxlen=200)
        # Auxiliary per-episode metrics (kills, drops picked up, damage,
        # avg loadout rarity at death, …). Bots push values in via
        # `record_episode_extras` on death; run.py reads the per-metric
        # mean to maintain a separate `<base>.best.<metric>` checkpoint
        # alongside the score-based `<base>.best`, so we can keep multiple
        # candidate models and pick whichever metric best matches the
        # behavior we're trying to improve. Auto-creates deques on first
        # write so adding a new metric doesn't need any agent-side change.
        self._episode_extras: dict[str, deque[float]] = {}

        # In-process inter-agent comm path: every bot writes its own latest
        # state+action here when it decides, and other bots read the K
        # nearest entries when building their observation. This is the path
        # by which one bot's model output flows into another bot's input —
        # equivalent to a CommNet-style learned signal channel, but using a
        # simple shared dict because all bots in this launcher share the
        # same agent instance. Cross-process setups can fall back on the
        # chat data channel (encode_position / encode_kill / etc.).
        self.peer_messages: dict[str, dict] = {}

        self.checkpoint_path = checkpoint_path
        if checkpoint_path and os.path.exists(checkpoint_path):
            try:
                state = torch.load(checkpoint_path, map_location=self.device)
                pad_report = _pad_load_state_dict(self.q, state["q"])
                # Mirror into the target *and* inference nets so they all
                # start in sync with the loaded weights.
                self.target.load_state_dict(self.q.state_dict())
                self.q_inference.load_state_dict(self.q.state_dict())
                self.env_steps = state.get("env_steps", 0)
                self.train_steps = state.get("train_steps", 0)
                msg = f"[agent] loaded checkpoint {checkpoint_path} (env_steps={self.env_steps}, train_steps={self.train_steps})"
                if pad_report["padded"] or pad_report["skipped"]:
                    msg += (f"  pad-loaded {len(pad_report['padded'])} layers "
                            f"with shape mismatch (e.g. action-space grew); "
                            f"new outputs init random. Skipped: {pad_report['skipped']}")
                print(msg)
            except Exception as e:  # noqa: BLE001
                print(f"[agent] failed to load checkpoint {checkpoint_path}: {e}; starting fresh")

    # ---- inter-agent comm ----------------------------------------------

    def publish(self, name: str, payload: dict) -> None:
        """Bot calls this every action with its own latest state+output.
        Overwrites any previous payload for that name. Cheap: the agent
        is single-threaded inside the asyncio event loop."""
        self.peer_messages[name] = payload

    def read_peers(
        self,
        exclude_name: str,
        my_x: float,
        my_y: float,
        max_age: float = PEER_MESSAGE_TTL,
    ) -> list[dict]:
        """Return peer payloads sorted by distance from (my_x, my_y), with
        stale entries (older than max_age wall-seconds) filtered out. Used by
        the bot to pull the K nearest comm slots into its observation."""
        import time as _t
        now = _t.time()
        out: list[tuple[float, dict]] = []
        for name, msg in self.peer_messages.items():
            if name == exclude_name:
                continue
            ts = msg.get("time", 0.0)
            if now - ts > max_age:
                continue
            dx = msg.get("x", 0.0) - my_x
            dy = msg.get("y", 0.0) - my_y
            out.append((dx * dx + dy * dy, msg))
        out.sort(key=lambda t: t[0])
        return [m for _, m in out]

    def record_episode(self, score: int, reward: float) -> None:
        self._episode_scores.append(int(score))
        self._episode_rewards.append(float(reward))

    def episode_window(self) -> tuple[int, float, float, float, float]:
        """Returns (n, score_mean, score_max, reward_mean, reward_max)."""
        n = len(self._episode_scores)
        if n == 0:
            return 0, 0.0, 0.0, 0.0, 0.0
        return (
            n,
            sum(self._episode_scores) / n,
            float(max(self._episode_scores)),
            sum(self._episode_rewards) / n,
            float(max(self._episode_rewards)),
        )

    def record_episode_extras(self, extras: dict[str, float]) -> None:
        """Push per-episode auxiliary metrics. Auto-creates a 200-deep
        rolling deque per metric name on first write. Called by
        `LearningBot._on_death` after the primary `record_episode`."""
        for name, value in extras.items():
            dq = self._episode_extras.get(name)
            if dq is None:
                dq = deque(maxlen=200)
                self._episode_extras[name] = dq
            dq.append(float(value))

    def extra_window_mean(self, name: str) -> tuple[int, float]:
        """Mean of the rolling window for a named extra metric. Returns
        (sample_count, mean). `(0, 0.0)` if no samples yet."""
        dq = self._episode_extras.get(name)
        if not dq:
            return 0, 0.0
        return len(dq), sum(dq) / len(dq)

    def extra_metric_names(self) -> list[str]:
        """Names of extras the bots have reported at least once. Lets
        run.py iterate without hardcoding the metric list."""
        return list(self._episode_extras.keys())

    def epsilon(self) -> float:
        ratio = min(1.0, self.env_steps / max(1, self.eps_decay_steps))
        return self.eps_start + (self.eps_end - self.eps_start) * ratio

    @torch.no_grad()
    def act(self, state: list[float], greedy: bool = False) -> int:
        if not greedy and random.random() < self.epsilon():
            return random.randrange(NUM_ACTIONS)
        # CPU tensor on purpose — q_inference lives on CPU (see __init__).
        s = torch.as_tensor(state, dtype=torch.float32).unsqueeze(0)
        # Inference uses `q_inference`, the periodically-synced read-only
        # copy. The trainer only touches it under `_sync_lock`, so a brief
        # acquire here gives us a stable snapshot. Forward pass itself
        # releases the GIL during the matmul, so the trainer thread can
        # keep working while we're computing the argmax.
        with self._sync_lock:
            return int(self.q_inference(s).argmax(dim=1).item())

    def push(self, s: list[float], a: int, r: float, s2: list[float], done: bool) -> None:
        # Buffer is read by the trainer thread (row gather in _train_step).
        # Lock the write so a sample can't see a half-written row. Bumping
        # env_steps is also covered by the lock — although it's a single
        # int and atomic in CPython under GIL, the trainer reads it as a
        # gating signal and we want the buffer length and env_steps to be
        # observed consistently. The list → float32 row conversion happens
        # here, once, so the trainer never touches Python objects.
        with self._buffer_lock:
            i = self._buf_pos
            self._buf_s[i] = s
            self._buf_a[i] = a
            self._buf_r[i] = r
            self._buf_s2[i] = s2
            self._buf_d[i] = 1.0 if done else 0.0
            self._buf_pos = (i + 1) % self.buffer_size
            if self._buf_len < self.buffer_size:
                self._buf_len += 1
            self.env_steps += 1
        # Note: this used to trigger `_train_step()` synchronously every
        # `train_every` pushes. That path is gone — the trainer thread
        # now owns the training cadence (see `_trainer_loop`), so push
        # itself is dirt-cheap and never blocks the asyncio event loop.

    # ---- trainer thread -------------------------------------------------

    def start_trainer(self) -> None:
        """Spin up the background trainer thread. Idempotent — safe to call
        multiple times. Should be called by run.py after the agent is
        constructed so training begins as soon as warmup is reached."""
        if self._trainer_thread is not None and self._trainer_thread.is_alive():
            return
        self._stop_event.clear()
        self._trainer_thread = threading.Thread(
            target=self._trainer_loop, daemon=True, name="dqn-trainer"
        )
        self._trainer_thread.start()

    def stop_trainer(self, timeout: float = 5.0) -> None:
        """Signal the trainer thread to exit and wait for it to finish.
        Idempotent. Called by run.py during shutdown."""
        self._stop_event.set()
        t = self._trainer_thread
        if t is not None:
            t.join(timeout=timeout)
        self._trainer_thread = None

    def _trainer_loop(self) -> None:
        """Run on a daemon thread for the lifetime of the agent. Pulls
        batches from the replay buffer as fast as the configured
        `train_every` cadence allows, performs gradient updates on `q`,
        and periodically syncs the inference / target nets."""
        last_train_env_step = 0
        while not self._stop_event.is_set():
            # Snapshot buffer-side counters under the lock so we don't
            # trip on a torn read of `env_steps` mid-increment.
            with self._buffer_lock:
                buf_size = self._buf_len
                cur_env = self.env_steps
            ready = (
                cur_env >= self.warmup
                and cur_env >= last_train_env_step + self.train_every
                and buf_size >= self.batch_size
            )
            if ready:
                self._train_step()
                last_train_env_step = cur_env
            else:
                # Briefly idle so we don't burn CPU spinning. 5 ms is a
                # comfortable lower bound on training cadence even at
                # high control_hz.
                self._stop_event.wait(timeout=0.005)

    def _train_step(self) -> None:
        # Lock the buffer for the *gather* only — not for the tensor work
        # that follows. The fancy-index copies the sampled rows out of the
        # ring (microseconds, vectorized, no Python objects), so the lock
        # is released before the multi-millisecond forward+backward and
        # push() never waits on a gradient step.
        with self._buffer_lock:
            idx = np.random.randint(0, self._buf_len, size=self.batch_size)
            s = self._buf_s[idx]
            a = self._buf_a[idx]
            r = self._buf_r[idx]
            s2 = self._buf_s2[idx]
            d = self._buf_d[idx]
        # from_numpy is zero-copy; .to(device) is the single host→device
        # transfer per array (a no-op when training on CPU).
        s_t = torch.from_numpy(s).to(self.device)
        a_t = torch.from_numpy(a).unsqueeze(1).to(self.device)
        r_t = torch.from_numpy(r).to(self.device)
        s2_t = torch.from_numpy(s2).to(self.device)
        d_t = torch.from_numpy(d).to(self.device)

        q_pred = self.q(s_t).gather(1, a_t).squeeze(1)
        with torch.no_grad():
            # Double DQN: decouple action *selection* (online net) from
            # action *evaluation* (target net). Vanilla DQN's max over
            # the target net systematically overestimates Q-values
            # because the same network both picks and evaluates the
            # argmax — any noise that makes one action look spuriously
            # good gets propagated as the bootstrap target. Especially
            # bad in multi-agent .io-style environments where rewards
            # are non-stationary (opponents' policies shift mid-training)
            # and outlier rewards (W_PLAYER_KILL_BONUS=200, camp/weak
            # stacks up to +350) get amplified by the overestimation.
            # The one-extra forward through self.q is cheap (we're
            # already inside no_grad) and the stability win is large.
            a_next = self.q(s2_t).argmax(dim=1, keepdim=True)
            q_next = self.target(s2_t).gather(1, a_next).squeeze(1)
            q_target = r_t + self.gamma * q_next * (1.0 - d_t)
        # Huber loss is more stable than MSE under reward outliers (kill bonuses).
        loss = nn.functional.smooth_l1_loss(q_pred, q_target)

        self.optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(self.q.parameters(), 10.0)
        self.optimizer.step()

        self.train_steps += 1
        # `.item()` forces a host↔device sync (~0.6 ms on MPS), and the
        # value is only read once per stats interval — sample it at the
        # inference-sync cadence instead of every step. Doubles as
        # backpressure: the periodic sync keeps the trainer from
        # enqueueing unboundedly far ahead of the GPU.
        if self.train_steps % self._inference_sync_every == 0:
            self.last_loss = float(loss.item())
        if self.train_steps % self.target_sync_steps == 0:
            # Target net is read+written exclusively by this thread, so no
            # lock is needed; just blow away the old weights with current.
            self.target.load_state_dict(self.q.state_dict())
        # Periodically copy q → q_inference so act() sees fresh policy
        # weights. The brief lock here is the *only* point where the
        # asyncio thread blocks on the trainer.
        if self.train_steps % self._inference_sync_every == 0:
            with self._sync_lock:
                self.q_inference.load_state_dict(self.q.state_dict())
        if (self.checkpoint_path
                and self.checkpoint_every_train_steps > 0
                and self.train_steps % self.checkpoint_every_train_steps == 0):
            self.save()
        # Federated peer sync — average online net's weights with the
        # other workers' saved checkpoints at the configured cadence.
        # This must run *after* the local save so our own slot reflects
        # the latest pre-merge weights before sibling workers pick it up,
        # and so a tight checkpoint_every / peer_sync_every coupling
        # doesn't read stale data from disk on the very tick we'd otherwise
        # be writing fresh.
        if (self._peer_sync_paths_provider is not None
                and self._peer_sync_every_train_steps > 0
                and self.train_steps % self._peer_sync_every_train_steps == 0):
            paths = self._peer_sync_paths_provider()
            if paths:
                report = self.merge_peer_weights(
                    paths, my_weight=self._peer_sync_my_weight
                )
                if self._peer_sync_log and report["peers_merged"] > 0:
                    print(
                        f"[agent] peer-sync merged "
                        f"{report['peers_merged']}/{len(paths)} peer(s) "
                        f"at train_step={self.train_steps}",
                        flush=True,
                    )

    def save(self) -> None:
        if not self.checkpoint_path:
            return
        self.save_to(self.checkpoint_path)

    def save_to(self, path: str) -> None:
        """Snapshot the live Q-network to `path`. Used both for the
        normal periodic checkpoint and for the multi-worker "best so
        far" file maintained in run.py. Holds the sync lock so we
        capture a self-consistent view of the weights (no half-applied
        gradient step). We save `q` not `q_inference` because q is the
        freshest set of weights; on resume we want the latest gradient
        update, not the last sync point."""
        with self._sync_lock:
            sd = {k: v.cpu().clone() for k, v in self.q.state_dict().items()}
            env_steps = self.env_steps
            train_steps = self.train_steps
        torch.save(
            {"q": sd, "env_steps": env_steps, "train_steps": train_steps},
            path,
        )

    def configure_peer_sync(
        self,
        paths_provider,
        every_train_steps: int,
        my_weight: float = 0.5,
        log: bool = True,
    ) -> None:
        """Enable federated-averaging-style sync with sibling workers.
        `paths_provider` is a zero-arg callable returning the list of
        per-worker checkpoint paths to read (excluding our own); typically
        wired up by run.py from `_worker_path(base, w)` for each peer
        worker id. `every_train_steps` controls the cadence (0 disables).
        `my_weight` is how much of our own weights survive the merge
        (0.5 = equal blend with the peer-average; closer to 1.0 keeps
        more of our trajectory, closer to 0.0 pulls harder toward
        consensus)."""
        self._peer_sync_paths_provider = paths_provider
        self._peer_sync_every_train_steps = max(0, int(every_train_steps))
        self._peer_sync_my_weight = max(0.0, min(1.0, float(my_weight)))
        self._peer_sync_log = bool(log)

    def merge_peer_weights(self, paths: list, my_weight: float = 0.5) -> dict:
        """Read each path's saved state_dict and average our online net's
        weights with theirs:  q := my_weight * q + (1 - my_weight) * mean(peers).
        Skips any path that's missing, mid-write, or shape-incompatible
        (e.g. a peer hasn't grown to the new QNet size yet). Merge runs
        under `_sync_lock` so the inference copy can't pull torn weights.

        Returns {"peers_merged": int} so callers can log the merge."""
        own_sd = {k: v.clone() for k, v in self.q.state_dict().items()}
        peer_sds = []
        for path in paths:
            try:
                data = torch.load(path, map_location=self.device)
            except (FileNotFoundError, EOFError, RuntimeError, OSError):
                # Missing file (peer hasn't saved yet) or torn read
                # (peer is mid-write); skip silently — we'll retry next
                # sync cycle.
                continue
            if not isinstance(data, dict):
                continue
            peer_sd = data.get("q")
            if peer_sd is None:
                continue
            # Reject shape-mismatched peers outright — e.g. one worker on
            # an old QNet size after a hot reload. Mixing partial slices
            # gives garbage; better to skip and re-converge next cycle.
            if not all(
                k in peer_sd and peer_sd[k].shape == own_sd[k].shape
                for k in own_sd
            ):
                continue
            peer_sds.append(peer_sd)
        if not peer_sds:
            return {"peers_merged": 0}
        other_weight = (1.0 - my_weight) / len(peer_sds)
        merged_sd = {}
        for k, own_t in own_sd.items():
            acc = own_t * my_weight
            for peer_sd in peer_sds:
                acc = acc + peer_sd[k].to(self.device, dtype=own_t.dtype) * other_weight
            merged_sd[k] = acc
        with self._sync_lock:
            self.q.load_state_dict(merged_sd)
            # Also refresh q_inference so bots start using the merged
            # weights immediately instead of waiting for the next
            # _inference_sync_every cycle.
            self.q_inference.load_state_dict(merged_sd)
        return {"peers_merged": len(peer_sds)}
