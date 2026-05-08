# gardn learning bots

Standalone Python bots that connect to a gardn server, learn to play via
deep Q-learning, and earn score by killing mobs and other players. They
speak the wire protocol directly — no headless browser, no JavaScript.

## Quick start

```sh
# 1. start a server (from repo root)
cd Server/build && node ./gardn-server.js &

# 2. install deps (websockets + torch)
pip3 install -r Bots/requirements.txt

# 3. spawn a swarm and start learning
python3.11 Bots/run.py -n 6
```

`Ctrl-C` to stop. The model autosaves every 1 000 gradient updates to
`Bots/model.pt`; restarting `run.py` resumes from there.

> Use the Python interpreter that has `torch` installed. On this machine
> that's `python3.11` — adjust as needed.

## CLI

```
python3.11 run.py [-n N] [-u URL] [-s STAGGER]
                  [--lr LR] [--gamma G] [--eps-decay STEPS] [--warmup STEPS]
                  [--device DEV] [--checkpoint PATH] [--stats-interval SEC]

  -n, --count            number of bots (default: 4)
  -u, --url              WebSocket URL (default: ws://localhost:9001)
  -s, --stagger          seconds between successive spawns (default: 0.25)
  --lr                   Adam learning rate (default: 5e-4)
  --gamma                discount factor (default: 0.97)
  --eps-decay            env-steps over which ε goes 1.0 → 0.05 (default: 30 000)
  --warmup               env-steps to collect before training starts (default: 2 000)
  --device               torch device: cpu / cuda / mps (default: cpu)
  --checkpoint PATH      load/save the QNet weights here ('' to disable)
  --stats-interval       seconds between stats prints (default: 10)
  --control-hz           actions per second per bot (default: 25). Raise this
                         when running the server at higher TPS so the agent
                         actually samples the faster simulation.
  --action-repeat N      hold each agent decision for N control ticks before
                         re-deciding (default: 1). At TPS=400 try 4.
```

## What's actually learning

A single shared DQN drives every bot in the process:

- **State (25 floats):** 13 "world" features + 12 "comm" features.
  - World (13): self HP ratio + relative `(dx, dy)`, is-player flag, and HP
    for the three nearest hostiles (zero-padded if fewer in view). Distances
    are scaled by 1500 so inputs stay in roughly `[-1, 1]`.
  - Comm (12): two peer slots (the K=2 nearest bots that have published
    recently), each carrying `(rel_dx, rel_dy, peer_hp, peer_vx, peer_vy,
    peer_attacking)`. The `peer_vx, peer_vy, peer_attacking` triple is the
    **other bot's most recent model output** decoded the same way that
    bot's own action is mapped to motion — so this slot is literally one
    agent's QNet output flowing into another agent's QNet input. See
    "Inter-agent communication" below.
- **Actions (18 discrete):** `{stay, N, NE, E, SE, S, SW, W, NW}` × `{attack, defend}`.
  Defend in this game pulls petals into a tight cluster around the player
  (high single-target DPS, no orbital coverage), so attack-vs-defend is a
  real strategic choice rather than a fixed rule.
- **Reward:** `Δscore + 5·Δhp − 0.01` per tick, plus `−10` on death.
  Score deltas are clipped to non-negative so the respawn-XP grant doesn't
  get credited as a "reward" for dying. **PvP shaping on top of that:**
  - If `Δscore ≥ PVP_KILL_THRESHOLD` (40) — too big to be a mob kill, a
    player kill pays half the dead player's score so jumps are typically
    50+ — the score reward is multiplied by `1 + W_PVP_BONUS = 3` for that
    tick. Player kills are by far the most lucrative thing the policy can
    learn.
  - While at least one hostile-team Flower is within `PROXIMITY_RANGE` of
    the bot, `+W_PROXIMITY = +0.05` per tick. Continuous gradient toward
    "be near a fight" so the policy seeks PvP even before any kill resolves.
    Tiny per tick (~+2.5/sec at control_hz=50, well under typical mob-kill
    rewards) so it can't dominate the score signal — it's just a nudge.
  - **Damage dealt** (the strongest of the three): every tick we snapshot
    every visible hostile (mob *or* enemy player) — `(hp, x, y, radius)` —
    plus the positions of our orbiting petals. Two paths credit damage:
    1. *Visible enemies whose HP just dropped.* Check whether any of our
       current petals overlaps the enemy (sum-of-radii squared compare,
       same physics the server's collision system uses). If yes, credit
       the drop. This catches every hit on something still alive in our
       view.
    2. *Enemies that vanished from view since last tick.* Between two
       control-loop ticks the server can ship both the kill packet and the
       subsequent deletion, so by the time we look the dying entity is
       already gone. We use the snapshot's last-known position *plus the
       snapshot's petal positions* (taken at the same moment) to test
       overlap. Any disappeared entity that was overlapping a snapshotted
       petal counts as our kill, crediting whatever HP was left.

    Reward: `W_DAMAGE = 4.0` per HP-ratio point credited. Mobs go down in
    1–2 hits (so a typical mob credit is the full 1.0), and players take
    many small drops over time — both end up rewarded fairly. Unlike a
    flat "in range of player" check, the per-petal collision matches what
    actually deals damage in this game; a sloppy radius around the player
    center either over-credits or misses kills depending on which way the
    petal happens to be orbiting. Per-life and lifetime cumulative
    attributed damage are exposed as `bot.damage_dealt` and
    `bot.damage_dealt_lifetime`.
  - **Approach** (dense gradient toward *any* hostile, including mobs):
    every tick, compute the distance to the nearest hostile in view and
    compare to last tick's distance. Pay `W_APPROACH × (closing / OBS_SCALE)`
    where `closing` is clamped to `±APPROACH_CAP` units per tick. The cap
    matters because killing a target swaps the "nearest" identity, and
    without it the raw distance jump would register as a huge "moved away"
    penalty that punishes the kill. With the cap, the term is purely about
    motion — symmetric, so the policy can't game it by intentionally
    abandoning targets. This is what stops the bots making jittery
    no-net-displacement movements: every action that closes distance gets a
    small immediate reward, every action that increases it gets a small
    penalty. The other PvP terms only fire once you're already engaged;
    this one shapes the *approach* phase. Per-life cumulative approach
    reward is exposed as `bot.approach_reward_total`.

  The 3-hostile slots in the state already include `is_player` flags, so
  the network has the info it needs to discriminate; the shaping creates
  the gradient that makes those slots worth attending to.
- **Algorithm:** vanilla DQN — 128-128 MLP, target network synced every 1 000
  gradient steps, Huber loss, gradient clipping. ε-greedy exploration
  decays linearly from 1.0 to 0.05 over `--eps-decay` env-steps.
- **Sharing:** all bots push transitions into one replay buffer and call
  `act()` on the same Q-net. With N bots you collect N× experience per
  wall-clock second, which is the main reason to spawn more.

## Inter-agent communication

There are two paths by which one bot's model output can reach another bot:

1. **In-process blackboard (this is the path the agent actually conditions
   on).** All bots in the launcher share the same `DQNAgent`. Each bot, every
   action, calls `agent.publish(name, {x, y, hp, action, time})`. When
   building its observation, every bot calls `agent.read_peers(...)` to pull
   the K nearest non-stale entries and concatenates them into the comm slots
   of its state vector. This is lossless, has no rate-limit, and runs at the
   full `--control-hz`. Stale entries (older than `PEER_MESSAGE_TTL = 5 s`
   wall-clock) are filtered out automatically, so dead/disconnected peers
   stop influencing observations within a few seconds.

2. **Chat data channel (cross-process / spectatable).** The same intent is
   serialised over the chat channel as binary messages
   (`encode_position`, `encode_kill`, `encode_help`, …). Slower (chat is
   rate-limited to one message per `CHAT_COOLDOWN_TICKS / SIM_RATE` game-
   seconds per client) and lossy (floats are protocol-quantised), but works
   between launcher processes that don't share memory. Used here for
   broadcast events like kills / help calls, and as a debug stream a human
   watching `run.py` can read.

Both paths can run simultaneously — the in-process one drives the policy,
the chat-data one is for cross-cutting events and observability.

If you change `STATE_DIM` (e.g. by tweaking `K_PEERS` or `COMM_PER_PEER`),
old `model.pt` checkpoints become incompatible. The agent prints a
shape-mismatch warning and starts fresh; you don't need to manually delete
the file but you can.

## Stats line

Every `--stats-interval` seconds the launcher prints something like:

```
[stats] env_steps=  6419  train_steps=  1105  eps=0.797  loss=0.001  episodes=  43  win200: ep_score(mean/max)=18.4/52  ep_R(mean/max)=+3.2/+18.6
```

- `env_steps` — total transitions pushed across all bots.
- `train_steps` — gradient updates performed (starts after `--warmup`).
- `eps` — current ε used by the swarm.
- `loss` — most-recent batch's smooth-L1 loss.
- `episodes` — total deaths across all bots so far.
- `win200: ep_score(mean/max)` — score-at-death over the **last 200 finished
  episodes across the swarm**. This is the curve to watch for actual
  learning. A rising mean means the policy is improving.
- `win200: ep_R(mean/max)` — total reward over the same window.

Until any bot has died, the line falls back to `live_score_mean=… (no episodes
finished yet)` so something prints during warmup.

> Don't trust `live_score_mean` (the score of currently-alive bots) on its own.
> When bots die fast — common at high TPS or against strong mobs — the
> currently-alive set is dominated by freshly-respawned bots near zero score
> regardless of policy quality. The rolling episode mean is the only honest
> signal.

## File layout

- `protocol.py` — varint reader/writer + entity create/update decoder + chat
  packet helpers. Field order **must** stay in lock-step with
  `Shared/EntityDef.hh`. Bump `VERSION_HASH` in `bot.py` whenever the server
  changes it in `Shared/Config.cc`, otherwise the server replies `kOutdated`
  and hangs up.
- `agent.py` — DQN network, replay buffer, training loop, save/load.
- `bot.py` — protocol I/O, observation builder, transition bookkeeping,
  chat triggers.
- `memory.py` — per-bot persistent JSON state + per-life episodic scratchpad.
- `run.py` — multi-bot launcher with shared agent + stats reporter.
- `model.pt` — DQN checkpoint (one shared across the swarm).
- `state/<bot-name>.json` — one persistent-memory file per bot identity.

## Memory and chat

Each bot now carries two scopes of memory:

- **Persistent (`Bots/state/<bot-name>.json`)**: JSON file keyed on the bot's
  display name. Survives process restarts. Tracks lifetime episode count,
  cumulative score, best single-episode score, the loadout that achieved that
  best score, and a `killed_by` tally (counts of which player or mob species
  killed this bot the most). Atomic write — `os.replace` after `fsync`, so
  killing the launcher mid-save doesn't corrupt the file.
- **Episodic (in-RAM)**: scratchpad reset every life. Holds the recent chat
  log, this-life score-deltas, and the most recent primary loadout we saw on
  ourselves (so on death we can credit `best_loadout` correctly).

Bots talk over the **server chat channel**. The server validates inbound
chat, stamps the sender from the camera's active player (so spectators can't
chat), enforces a per-client `CHAT_COOLDOWN_TICKS` rate limit (game-time, so
TPS-invariant), and rebroadcasts to every connected client in the
`GameInstance`.

The chat field is a UTF-8 string but the bots **piggyback a structured
binary protocol on top of it**: any line whose first code unit is `\x01`
(SOH; valid UTF-8 single-byte, accepted by the server's UTF8Parser because
any byte < 0x80 passes) is a base64-encoded binary message using the same
varint/float Writer/Reader as the rest of the wire protocol. Five message
kinds, all under 14 chars on the wire:

| kind | payload | trigger |
| --- | --- | --- |
| `position` (1) | `float x, float y, float hp` | every ~2 s wall-clock |
| `kill`     (2) | `u32 score_delta, float x, float y` | score delta ≥ 20 in one tick |
| `help`     (3) | `float x, float y, float hp` | first time HP < 30 % per life |
| `target`   (4) | `eid target, float x, float y` | reserved for cooperative pursuit |
| `threat`   (5) | `u8 mob_id, float x, float y` | reserved for danger callouts |

Plain text is still supported alongside data chat — the respawn-quip on
return uses persistent stats and goes out as readable English (`back. 20
lives so far`, `pb 90, beating it today`, `watching for Beetle (2× killer)`).
Anything not starting with `\x01` is treated as plain human chat.

The launcher prints inbound traffic to stdout, distinguishing the two
channels:

```
<chat> [petalpop] back. 8 lives so far
<data> [stamen]   position: x=10253.94, y=1416.23, hp=0.3125
<data> [thorny]   kill: score=27, x=2893.88, y=1164.31
```

Bots skip echoing their own messages so the log stays readable. Decoded
data messages are stored in `episodic.peer_state[sender_name][kind] =
(recv_time, payload)` so each bot has a live read of every other bot's
self-reported state — ready to feed into a future state representation.

To wipe a bot's memory: `rm Bots/state/<bot-name>.json`. To wipe everything:
`rm -rf Bots/state Bots/model.pt`.

## Faster training: raise the server tick rate

The default server runs at 20 ticks/sec, which caps how much game-time the
bots can experience per wall-clock second. The build now exposes a
`-DTPS=N` CMake flag so you can crank it up. The native server handles much
higher tick rates than the WASM/Node server.

> **TPS vs SIM_RATE.** Gameplay timing — AI state durations, petal reloads,
> mob movement, despawn timers — is calibrated against a fixed
> `SIM_RATE = 20` constant (defined in `Shared/StaticData.cc`). `TPS` only
> controls how often the server schedules a `tick()` call. Bumping TPS
> compresses the same game-time into less wall-clock-time (`TPS / SIM_RATE` ×
> speedup); it does **not** make mobs move faster per game-second. So a mob
> covers the same game-distance per AI cycle regardless of TPS — only the
> wall-clock time taken shrinks. This is the right behavior for fast training:
> per-game-second mechanics stay at the original balance, but you can stuff
> more game-seconds into a real second.

```sh
# native server at TPS=200 (10× normal speed)
cd Server
mkdir -p build && cd build
cmake -DTPS=200 ..      # add -DWASM_SERVER=1 to keep using the WASM build
make
./gardn-server          # native binary; or `node ./gardn-server.js` for WASM

# match the bot's sampling rate to the higher TPS
python3.11 ../../Bots/run.py -n 6 --control-hz 50
```

Rule of thumb: keep `--control-hz` at roughly `TPS/4` so each action lasts a
handful of server ticks (action-repeat is good for stable Q-learning), and
roughly proportional to the speedup you want. With `TPS=200` and
`--control-hz 50`, the swarm collects ~2–3× the env-steps/sec compared to
the defaults, and bot deaths (which give the strongest reward signal) start
showing up much sooner.

At very high TPS (400+) the bots can sample faster than meaningful in-game
events resolve — petals reload over many ticks, mobs move slowly per tick,
etc. Picking a fresh action every 20 ms accumulates a lot of jittery
"explore" frames whose reward is essentially noise, which makes credit
assignment harder. The fix is `--action-repeat N`: hold each agent decision
for N ticks before re-deciding. Transitions are still pushed every tick (so
the buffer fills at full rate) but the agent is forced to commit to a
direction long enough to feel its consequence. Try `--action-repeat 4` at
TPS=400 with `--control-hz 50` (so the agent decides ≈12.5×/sec, each
action holding for ≈80 ms ≈ 32 game-ticks).

If you've trained for a while and `win200: ep_score (mean)` is flat, the
likely culprits are:
1. **No action repeat** at high TPS — start there.
2. **ε is still high** (early ε with a slow decay schedule). Look at the
   `eps=` field; if it's still > 0.3 after 100 k env-steps you're mostly
   exploring noise. Lower `--eps-decay`.
3. **Reward weights** in `bot.py` (`W_SCORE`, `W_HP`, `IDLE_PENALTY`,
   `DEATH_PENALTY`). The defaults are tuned for TPS=20; at higher rates
   `IDLE_PENALTY` accumulates much faster relative to score gains, biasing
   the policy toward "do nothing." Either lower it or bump `W_SCORE` to
   compensate.

If the client connects at the wrong TPS the world will appear sped up or
slowed down — rebuild the client with the same `-DTPS=N` flag if you want
to spectate at non-default rates. The bots themselves don't care about
client builds.

## Things to be aware of

- The world is 40 000 × 4 000, so two bots that spawn in different difficulty
  zones may never encounter each other. Spawn `-n 10+` if you want
  consistent PvP signal.
- TDM mode (server compiled with `-DTDM=1`) puts each bot on the team it's
  assigned. The friendly-team check uses `camera.team`, so PvP only fires
  on cross-team entities — bots will not attack their teammates.
- Drop pickup is automatic on the server (`_pickup_drop` in
  `Server/Process/Collision.cc`). Bots run a deterministic inventory manager
  alongside the DQN: every ~0.4 s, if a storage slot holds a strictly
  rarer petal than the worst primary slot, the bot sends `kPetalSwap` to
  promote it. Rarities come from the table baked into `protocol.py:PETAL_RARITY`,
  which mirrors `PETAL_DATA` in `Shared/StaticData.cc` — keep it in sync
  when new petals are added.
- `kPetalDelete` fires when both primary and storage are completely full
  (no `kNone` slot anywhere) — the server's auto-pickup needs an empty slot
  to deposit drops, so without deletion the bot eventually stops collecting.
  Picks the lowest-rank storage slot so the active loadout stays intact;
  refund XP comes back as score (Server/Client.cc applies a rarity-scaled
  bonus when a non-basic non-empty petal is deleted).
- Bots see the same arena/leaderboard a real client renders — `bot.arena`
  is a dict with `player_count`, `scores[10]`, `names[10]`, `colors[10]`,
  kept in sync with the server's `Arena` via the trailer on every
  `kClientUpdate` packet.
- Real DQN convergence on a problem this open takes 100k+ env-steps minimum
  (≈ several hours with 6 bots). The first hour mostly looks like noise.
  Let it run and watch `last_score (mean)` climb.
- To delete the trained policy and start over: `rm Bots/model.pt`.
