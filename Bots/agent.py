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
# State layout is **append-only** with respect to previous versions so
# pad_load_state_dict can preserve trained weights from older checkpoints.
# Append, never reorder. Boundaries:
#   [HP] [hostile×12] [loadout_rank×16] [peer_comm×12]            |  ← old 41
#       [loadout_type×16] [drops×12]                              |  ← old 69
#       [loadout_burst×16]                                        |  ← +16 = 85
STATE_DIM = (
    BASE_STATE_DIM            # 13
    + LOADOUT_FEAT_DIM        # 16
    + COMM_DIM                # 12
    + LOADOUT_TYPE_DIM        # 16
    + DROP_FEAT_DIM           # 12
    + LOADOUT_BURST_DIM       # 16
)                              # = 85
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
        self.net = nn.Sequential(
            nn.Linear(STATE_DIM, 128),
            nn.ReLU(),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, 128),
            nn.ReLU(),
            nn.Linear(128, NUM_ACTIONS),
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
        device: str = "mps",
        checkpoint_path: str | None = None,
        checkpoint_every_train_steps: int = 1_000,
    ) -> None:
        self.device = torch.device(device)
        # `q` is the live, training-mutated network — only the trainer
        # thread writes to it. `q_inference` is a periodically-synced copy
        # used by `act()` on the asyncio thread. Splitting them lets the
        # control loop's forward passes proceed without contending on the
        # weights the trainer is updating; the only point of contention
        # is the brief sync that copies state_dict() between them.
        self.q = QNet().to(self.device)
        self.q_inference = QNet().to(self.device)
        self.q_inference.load_state_dict(self.q.state_dict())
        self.q_inference.eval()
        self.target = QNet().to(self.device)
        self.target.load_state_dict(self.q.state_dict())
        self.target.eval()
        self.optimizer = optim.Adam(self.q.parameters(), lr=lr)

        self.gamma = gamma
        self.buffer: deque = deque(maxlen=buffer_size)
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

        # Rolling window of recent finished-episode results across the entire
        # swarm. Bots call `record_episode()` on death. The mean over this
        # window is the actual learning curve — `last_episode_*` on a single
        # bot is too noisy because every death overwrites it.
        self._episode_scores: deque[int] = deque(maxlen=200)
        self._episode_rewards: deque[float] = deque(maxlen=200)

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

    def epsilon(self) -> float:
        ratio = min(1.0, self.env_steps / max(1, self.eps_decay_steps))
        return self.eps_start + (self.eps_end - self.eps_start) * ratio

    @torch.no_grad()
    def act(self, state: list[float], greedy: bool = False) -> int:
        if not greedy and random.random() < self.epsilon():
            return random.randrange(NUM_ACTIONS)
        s = torch.as_tensor(state, dtype=torch.float32, device=self.device).unsqueeze(0)
        # Inference uses `q_inference`, the periodically-synced read-only
        # copy. The trainer only touches it under `_sync_lock`, so a brief
        # acquire here gives us a stable snapshot. Forward pass itself
        # releases the GIL during the matmul, so the trainer thread can
        # keep working while we're computing the argmax.
        with self._sync_lock:
            return int(self.q_inference(s).argmax(dim=1).item())

    def push(self, s: list[float], a: int, r: float, s2: list[float], done: bool) -> None:
        # Buffer is read by the trainer thread (random.sample). Lock the
        # append so it doesn't see a torn structure mid-rotation. Bumping
        # env_steps is also covered by the lock — although it's a single
        # int and atomic in CPython under GIL, the trainer reads it as a
        # gating signal and we want the buffer length and env_steps to be
        # observed consistently.
        with self._buffer_lock:
            self.buffer.append((s, a, float(r), s2, bool(done)))
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
                buf_size = len(self.buffer)
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
        # Lock the buffer for the *sample* only — not for the tensor work
        # that follows. Sampling is fast (microseconds); we don't want to
        # hold the lock across the multi-millisecond forward+backward.
        with self._buffer_lock:
            batch = random.sample(self.buffer, self.batch_size)
        s, a, r, s2, d = zip(*batch)
        s_t = torch.as_tensor(s, dtype=torch.float32, device=self.device)
        a_t = torch.as_tensor(a, dtype=torch.int64, device=self.device).unsqueeze(1)
        r_t = torch.as_tensor(r, dtype=torch.float32, device=self.device)
        s2_t = torch.as_tensor(s2, dtype=torch.float32, device=self.device)
        d_t = torch.as_tensor(d, dtype=torch.float32, device=self.device)

        q_pred = self.q(s_t).gather(1, a_t).squeeze(1)
        with torch.no_grad():
            q_next = self.target(s2_t).max(dim=1).values
            q_target = r_t + self.gamma * q_next * (1.0 - d_t)
        # Huber loss is more stable than MSE under reward outliers (kill bonuses).
        loss = nn.functional.smooth_l1_loss(q_pred, q_target)

        self.optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(self.q.parameters(), 10.0)
        self.optimizer.step()

        self.train_steps += 1
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

    def save(self) -> None:
        if not self.checkpoint_path:
            return
        # Hold the sync lock so we capture a self-consistent view of the
        # weights (no half-applied gradient step). We save `q` not
        # `q_inference` because q is the freshest set of weights; if the
        # process is killed we want to resume from the latest gradient
        # update, not the last sync point.
        with self._sync_lock:
            sd = {k: v.cpu().clone() for k, v in self.q.state_dict().items()}
            env_steps = self.env_steps
            train_steps = self.train_steps
        torch.save(
            {"q": sd, "env_steps": env_steps, "train_steps": train_steps},
            self.checkpoint_path,
        )
