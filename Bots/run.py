"""Spawn N learning bots that share one DQN agent.

Two modes:
  - Single-process (default): one DQNAgent, all bots share its replay buffer
    and Q-network. Experience pools across the swarm — learning is roughly
    linear in `--count` for the early curve where exploration dominates.
  - Multi-process (`--workers W > 1`): the parent forks W independent
    workers, each running `count/W` bots with its own agent. CPU usage
    scales linearly across cores (Python's GIL otherwise pins the whole
    fleet to one core). Each worker saves its own checkpoint at
    `<base>.W{id}` and competes for `<base>.best` — whichever worker has
    the highest rolling score_mean wins the best slot. All workers ALWAYS
    start from `<base>.best` if it exists, so good progress accumulates
    across runs and a struggling worker can hop onto the lead worker's
    weights at the next restart.
"""

from __future__ import annotations

import argparse
import asyncio
import gc
import json
import os
import random
import statistics
import string
import subprocess
import sys
import time

from agent import DQNAgent, _pad_load_state_dict
from bot import LearningBot

DEFAULT_NAMES = [
    "hello", "gardn", "florrio", "root", "leafy", "blossom", "rosie", "a", "aa", "67", "bot", "not bot", "thorn", "petalpop", "zorr", "blossom-v2", "flower", "thorny", "prickly", "bloom", "bloomy", "thornbloom",
]


def _make_name(i: int) -> str:
    if i < len(DEFAULT_NAMES):
        return DEFAULT_NAMES[i]
    suffix = str(i)
    return f"bot-{suffix}"


# -----------------------------------------------------------------------------
# Multi-worker checkpoint paths
#
# Given a base path like /path/to/model.pt:
#   <base>          — what the user passed; the "starter" checkpoint.
#                     Workers fall back to this if .best does not exist.
#   <base>.best     — current swarm-wide best Q-network. Replaced atomically
#                     when any worker's rolling score_mean beats the existing
#                     best. ALWAYS the load source on next startup.
#   <base>.best.meta— sidecar JSON: { score_mean, env_steps, train_steps,
#                     worker_id, saved_at }. Used by other workers to decide
#                     whether their snapshot is worth promoting.
#   <base>.W{id}    — per-worker save (always overwritten by that worker on
#                     each periodic save, regardless of whether it's best).
#                     Useful for debugging individual workers.
# -----------------------------------------------------------------------------


def _best_path(base: str) -> str:
    return base + ".best"


def _best_meta_path(base: str) -> str:
    return base + ".best.meta"


def _worker_path(base: str, worker_id: int | None) -> str:
    if worker_id is None:
        return base
    return f"{base}.W{worker_id}"


def _read_best_metric(base: str | None) -> float:
    if base is None:
        return float("-inf")
    meta = _best_meta_path(base)
    if not os.path.exists(meta):
        return float("-inf")
    try:
        with open(meta) as f:
            data = json.load(f)
        return float(data.get("score_mean", float("-inf")))
    except (OSError, json.JSONDecodeError):
        return float("-inf")


def _maybe_update_best(base: str | None, agent: DQNAgent, my_metric: float, worker_id: int | None) -> bool:
    """Atomically promote the agent's current weights to <base>.best if
    `my_metric` exceeds whatever score is recorded in <base>.best.meta.
    Returns True if the promotion happened."""
    if base is None:
        return False
    existing = _read_best_metric(base)
    if my_metric <= existing:
        return False
    best = _best_path(base)
    tmp = best + ".tmp"
    agent.save_to(tmp)
    os.replace(tmp, best)
    meta = _best_meta_path(base)
    tmp_meta = meta + ".tmp"
    with open(tmp_meta, "w") as f:
        json.dump({
            "score_mean": my_metric,
            "saved_at": time.time(),
            "worker_id": worker_id if worker_id is not None else -1,
            "env_steps": agent.env_steps,
            "train_steps": agent.train_steps,
        }, f)
    os.replace(tmp_meta, meta)
    return True


def _load_path(base: str | None) -> str | None:
    """At startup, prefer <base>.best; fall back to <base>. Returning a
    path that doesn't exist yet is fine — DQNAgent's loader treats a
    missing path as "start from random init"."""
    if base is None:
        return None
    best = _best_path(base)
    if os.path.exists(best):
        return best
    return base


# -----------------------------------------------------------------------------
# Worker subprocess spawn (parent mode)
# -----------------------------------------------------------------------------


def _spawn_workers(args: argparse.Namespace) -> int:
    """Parent mode: fork N child workers, each running its share of bots.
    Splits `count` as evenly as possible. Forwards stdout/stderr inline so
    the user sees per-worker stats interleaved (each prefixes its lines
    with the worker id).

    Spawns workers with an `n × stagger`-second delay between them so the
    server doesn't see all N×workers connections arrive in one burst —
    the total connection span ends up identical to running `count` bots
    in a single process with the same `--stagger`."""
    base_per = args.count // args.workers
    extra = args.count % args.workers
    children: list[subprocess.Popen] = []
    print(f"[run] spawning {args.workers} workers (count={args.count}, "
          f"split ~{base_per} bot(s) per worker)")
    # Thread-cap per worker. PyTorch / OpenMP / MKL each default to
    # `os.cpu_count()` worker threads inside their op kernels, so without
    # capping a fleet of N workers ends up with N × cpu_count threads
    # contending for the same physical cores — the CPU pegs at 100 %
    # but most of that is context-switch overhead, not useful work.
    # Default each worker to 1 thread so cores divide evenly across
    # workers; user can override via `--threads-per-worker`.
    cores = os.cpu_count() or 1
    threads_per_worker = args.threads_per_worker
    if threads_per_worker <= 0:
        threads_per_worker = max(1, cores // max(1, args.workers))
    print(f"[run] capping each worker to {threads_per_worker} torch / OMP "
          f"thread(s) (system has {cores} cores)")
    worker_env = os.environ.copy()
    worker_env["OMP_NUM_THREADS"] = str(threads_per_worker)
    worker_env["MKL_NUM_THREADS"] = str(threads_per_worker)
    worker_env["OPENBLAS_NUM_THREADS"] = str(threads_per_worker)
    worker_env["GARDN_TORCH_THREADS"] = str(threads_per_worker)
    for wid in range(args.workers):
        n = base_per + (1 if wid < extra else 0)
        if n <= 0:
            continue
        # Each worker waits `wid * n * stagger` seconds before opening its
        # first WebSocket so worker B starts spawning bots only after
        # worker A has finished its own staggered burst. Without this, all
        # workers race to connect simultaneously and a high-`-n` run can
        # overwhelm the server's accept loop on startup.
        worker_initial_delay = wid * base_per * args.stagger
        cmd = [
            sys.executable, os.path.abspath(sys.argv[0]),
            "--workers", "1",
            "--worker-id", str(wid),
            "-n", str(n),
            "-u", args.url,
            "-s", str(args.stagger),
            "--initial-delay", f"{worker_initial_delay:.3f}",
            "--control-hz", str(args.control_hz),
            "--action-repeat", str(args.action_repeat),
            "--lr", str(args.lr),
            "--gamma", str(args.gamma),
            "--eps-decay", str(args.eps_decay),
            "--warmup", str(args.warmup),
            "--device", args.device,
            "--stats-interval", str(args.stats_interval),
        ]
        if args.checkpoint:
            cmd += ["--checkpoint", args.checkpoint]
        else:
            cmd += ["--checkpoint", ""]
        children.append(subprocess.Popen(cmd, env=worker_env))
    rc = 0
    try:
        for p in children:
            r = p.wait()
            if r != 0:
                rc = r
    except KeyboardInterrupt:
        print("\n[run] forwarding shutdown to workers")
        for p in children:
            p.terminate()
        for p in children:
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
    return rc


# -----------------------------------------------------------------------------
# Single-process run loop (default mode, also used by each child worker)
# -----------------------------------------------------------------------------


async def _stats_loop(
    agent: DQNAgent,
    bots: list[LearningBot],
    interval: float,
    base_checkpoint: str | None,
    worker_id: int | None,
) -> None:
    while True:
        await asyncio.sleep(interval)
        eps_done = sum(b.episodes_finished for b in bots)
        n_win, score_mean, score_max, reward_mean, reward_max = agent.episode_window()

        snaps = [b.timing_snapshot() for b in bots]
        total_ticks = sum(s["tick_count"] for s in snaps)
        total_overruns = sum(s["overrun_count"] for s in snaps)
        total_work_s = sum(s["work_sum_s"] for s in snaps)
        max_work_s = max((s["max_work_s"] for s in snaps), default=0.0)
        realised_hz_per_bot = total_ticks / max(1e-6, interval) / max(1, len(bots))
        configured_hz = bots[0].control_hz if bots else 0.0
        overrun_pct = 100.0 * total_overruns / max(1, total_ticks)
        avg_work_ms = 1000.0 * total_work_s / max(1, total_ticks)
        max_work_ms = 1000.0 * max_work_s

        prefix = f"[stats W{worker_id}] " if worker_id is not None else "[stats] "
        line = (
            f"{prefix}env_steps={agent.env_steps:>6}  train_steps={agent.train_steps:>6}  "
            f"eps={agent.epsilon():.3f}  loss={agent.last_loss:.3f}  "
            f"episodes={eps_done:>4}  "
        )
        if n_win > 0:
            line += (
                f"win{n_win}: ep_score(mean/max)={score_mean:.1f}/{score_max:.0f}  "
                f"ep_R(mean/max)={reward_mean:+.1f}/{reward_max:+.1f}"
            )
        else:
            live_score_mean = statistics.mean(
                int(b._my_player().get("score", 0)) if b._my_player() else 0
                for b in bots
            )
            line += f"live_score_mean={live_score_mean:.1f} (no episodes finished yet)"
        line += (
            f"  | hz={realised_hz_per_bot:.1f}/{configured_hz:g}  "
            f"overrun={overrun_pct:.0f}%  avg_work={avg_work_ms:.1f}ms  "
            f"max_work={max_work_ms:.1f}ms"
        )
        rounds_seen = max((b.rounds_seen for b in bots), default=0)
        rounds_won = sum(b.rounds_won for b in bots)
        if rounds_seen > 0:
            line += f"  rounds={rounds_seen} wins={rounds_won}"
        print(line, flush=True)

        # Compete for the swarm-wide best slot. Only fires once we have a
        # real episode-window score to compare; live_score isn't stable
        # enough to use as a promotion criterion.
        if n_win > 0 and base_checkpoint:
            promoted = _maybe_update_best(base_checkpoint, agent, score_mean, worker_id)
            if promoted:
                print(
                    f"{prefix}new BEST score_mean={score_mean:.1f} "
                    f"(env_steps={agent.env_steps}); saved to {_best_path(base_checkpoint)}",
                    flush=True,
                )

        # Deterministic GC sweep, paired with `gc.disable()` at startup.
        # Runs once per stats interval so cycle-collection pauses happen
        # here (where we're already idle on `asyncio.sleep`) instead of
        # mid-tick. `gc.get_count()` returns (gen0, gen1, gen2) — if
        # gen2 is consistently 0, the collector has little to reclaim
        # and the manual cadence is fine; if it climbs, raise the
        # stats interval or call gc.collect() more often.
        if gc.isenabled() is False:  # i.e. we own the cadence
            gc.collect()


async def _amain(args: argparse.Namespace) -> None:
    # Belt-and-braces thread cap: env vars are set by the parent in
    # multi-worker mode, but a worker started without the parent (e.g.
    # `python run.py -n 4 --workers 1`) won't see them. Honor
    # GARDN_TORCH_THREADS if the parent set it; otherwise fall back to
    # --threads-per-worker; otherwise leave torch's default alone.
    _torch_threads = int(os.environ.get("GARDN_TORCH_THREADS", "0"))
    if _torch_threads <= 0:
        _torch_threads = args.threads_per_worker
    if _torch_threads > 0:
        try:
            import torch as _torch
            _torch.set_num_threads(_torch_threads)
        except Exception:  # noqa: BLE001
            pass

    # Manual GC. Each control tick allocates ~30 small Python objects
    # (feature lists, state tuples, replay-buffer entries). Aggregated
    # across a fleet of bots, that's millions of objects per minute.
    # CPython's gen-2 collector fires when those allocations cross
    # threshold[2]; the sweep walks the entire replay buffer (100k+
    # tuples) and stalls everything for tens-to-hundreds of ms — that's
    # the random `max_work` spike pattern showing up in the watchdog.
    #
    # Fix: switch off automatic generational GC and run `gc.collect()`
    # ourselves at deterministic points where a brief pause is harmless
    # (the stats loop, which is already in an `asyncio.sleep` cycle).
    # Manual collection still reclaims cycles so memory doesn't leak;
    # it just no longer happens at unpredictable, high-impact moments.
    if not args.no_gc_tuning:
        gc.disable()
        # Drain whatever's currently queued up before the bots open
        # sockets, so the first few ticks aren't loaded with the
        # startup-time allocations.
        gc.collect()

    base_checkpoint = args.checkpoint  # may be None
    # Always start from the best file when it exists; otherwise the
    # user's --checkpoint path. The worker then writes back to its own
    # per-worker file, NOT to base_checkpoint, so the original file is
    # only ever overwritten if a worker's metric exceeds the recorded
    # best (in which case .best gets updated, not the user's input).
    load_path = _load_path(base_checkpoint)
    save_path = _worker_path(base_checkpoint, args.worker_id) if base_checkpoint else None

    agent = DQNAgent(
        lr=args.lr,
        gamma=args.gamma,
        eps_decay_steps=args.eps_decay,
        warmup=args.warmup,
        device=args.device,
        checkpoint_path=save_path,
    )
    # Manually trigger a load from `load_path` if it differs from the
    # save_path the agent was constructed with. The agent's own loader
    # only consults checkpoint_path; for the multi-worker case we want
    # to load from .best but save to .W{id}.
    if load_path and load_path != save_path and os.path.exists(load_path):
        try:
            import torch as _torch
            state = _torch.load(load_path, map_location=agent.device)
            _pad_load_state_dict(agent.q, state.get("q", {}))
            agent.target.load_state_dict(agent.q.state_dict())
            agent.q_inference.load_state_dict(agent.q.state_dict())
            agent.env_steps = int(state.get("env_steps", 0))
            agent.train_steps = int(state.get("train_steps", 0))
            print(
                f"[agent W{args.worker_id}] loaded {load_path} (env_steps="
                f"{agent.env_steps}, train_steps={agent.train_steps})"
            )
        except Exception as e:  # noqa: BLE001
            print(f"[agent W{args.worker_id}] failed to load {load_path}: {e}; using fresh init")

    agent.start_trainer()

    bots = [
        LearningBot(
            _make_name(i + (args.worker_id or 0) * 100),
            agent=agent,
            url=args.url,
            control_hz=args.control_hz,
            action_repeat=args.action_repeat,
        )
        for i in range(args.count)
    ]
    # When the parent splits a fleet across workers, each child is given
    # an `--initial-delay` so its first WebSocket only opens after every
    # earlier worker has finished staggering its own bots. This keeps the
    # server's connection accept rate at ~1/stagger regardless of how
    # many workers we have.
    if args.initial_delay > 0:
        label = f" (worker {args.worker_id})" if args.worker_id is not None else ""
        print(f"[run]{label} delaying initial connection by {args.initial_delay:.2f}s")
        await asyncio.sleep(args.initial_delay)
    tasks = []
    for b in bots:
        tasks.append(asyncio.create_task(b.run_forever()))
        if args.stagger > 0:
            await asyncio.sleep(args.stagger)
    stats = asyncio.create_task(
        _stats_loop(agent, bots, args.stats_interval, base_checkpoint, args.worker_id)
    )
    label = f" (worker {args.worker_id})" if args.worker_id is not None else ""
    print(f"[run] spawned {args.count} learning bot(s){label} against {args.url}; ctrl-c to stop and save")
    try:
        await asyncio.gather(*tasks, stats)
    except asyncio.CancelledError:
        pass
    finally:
        agent.stop_trainer()
        # Save to per-worker path. The best slot was already maintained
        # incrementally by the stats loop, so we don't need a second
        # promotion check on shutdown.
        agent.save()


def main() -> int:
    p = argparse.ArgumentParser(description="Run learning bots against a gardn server.")
    p.add_argument("-n", "--count", type=int, default=4, help="number of bots (default: 4)")
    p.add_argument("-u", "--url", default="ws://localhost:9001", help="WebSocket URL")
    p.add_argument("-s", "--stagger", type=float, default=0.25, help="seconds between spawns")
    p.add_argument(
        "--control-hz", type=float, default=25.0,
        help="how many actions per second each bot picks; raise this when running the "
             "server at higher TPS (e.g. --control-hz 50 for a TPS=200 native build)",
    )
    p.add_argument(
        "--action-repeat", type=int, default=1,
        help="hold each agent decision for N control ticks before re-deciding. "
             "Helps DQN credit assignment at very high TPS where 1 tick is too "
             "short for any meaningful game-event to resolve. Try 4 at TPS=400.",
    )
    p.add_argument("--lr", type=float, default=5e-4, help="learning rate")
    p.add_argument("--gamma", type=float, default=0.97, help="discount factor")
    p.add_argument("--eps-decay", type=int, default=30_000, help="env steps over which ε decays from 1.0 to 0.05")
    p.add_argument("--warmup", type=int, default=2_000, help="env steps to collect before starting training")
    p.add_argument("--device", default="cpu", help="torch device (cpu / cuda / mps)")
    p.add_argument(
        "--checkpoint",
        default=os.path.join(os.path.dirname(__file__), "model.pt"),
        help="path to load/save the QNet weights (set to '' to disable persistence)",
    )
    p.add_argument("--stats-interval", type=float, default=10.0, help="seconds between stats prints")
    p.add_argument(
        "--workers", type=int, default=1,
        help="number of parallel worker subprocesses (default: 1 = single-process). "
             "Use this to scale across CPU cores; Python's GIL otherwise pins the whole "
             "bot fleet to one core. Each worker runs count/workers bots independently, "
             "competing for the swarm-wide best checkpoint at <checkpoint>.best.",
    )
    p.add_argument(
        "--worker-id", type=int, default=None,
        help="(internal) set by the parent when forking workers; identifies which "
             "worker this child is for checkpoint pathing.",
    )
    p.add_argument(
        "--initial-delay", type=float, default=0.0,
        help="(internal) seconds to wait before opening the first WebSocket. "
             "Used by the multi-worker spawner to space worker startup so the "
             "server doesn't see all N×workers connections in one burst.",
    )
    p.add_argument(
        "--threads-per-worker", type=int, default=0,
        help="cap each worker process to N torch / OMP threads. Default 0 = "
             "auto: floor(cpu_count / workers), at least 1. Without this, "
             "every worker spins up cpu_count threads and they all fight "
             "over the same cores — total CPU pegs at 100%% but most of "
             "that is context-switch overhead, not real work.",
    )
    p.add_argument(
        "--no-gc-tuning", action="store_true",
        help="disable the manual-GC mode that suppresses random per-tick "
             "stalls. Use this only when diagnosing memory leaks — the "
             "manual cadence (gc.collect at each stats interval) reclaims "
             "cycles fine, it just doesn't fire at random mid-tick moments "
             "the way CPython's default gen-2 sweep does.",
    )
    args = p.parse_args()
    if args.checkpoint == "":
        args.checkpoint = None

    # Parent mode: fork workers and wait. Children re-enter main() with
    # --workers 1 --worker-id N and fall through to the asyncio path.
    if args.workers > 1 and args.worker_id is None:
        return _spawn_workers(args)

    try:
        asyncio.run(_amain(args))
    except KeyboardInterrupt:
        print("\n[run] shutting down")
    return 0


if __name__ == "__main__":
    sys.exit(main())
