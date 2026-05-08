"""Spawn N learning bots that share one DQN agent.

All bots write transitions into the same replay buffer and consult the same
QNet, so experience pools across the swarm. This makes learning roughly
linear in `--count` for the early curve where exploration is the bottleneck.
"""

from __future__ import annotations

import argparse
import asyncio
import os
import random
import statistics
import string
import sys

from agent import DQNAgent
from bot import LearningBot

DEFAULT_NAMES = [
    "hello", "gardn", "florrio", "root", "leafy", "blossom", "rosie", "a", "aa", "67", "bot", "not bot", "thorn", "petalpop", "zorr"
]


def _make_name(i: int) -> str:
    if i < len(DEFAULT_NAMES):
        return DEFAULT_NAMES[i]
    suffix = "".join(random.choice(string.ascii_lowercase) for _ in range(4))
    return f"bot-{suffix}"


async def _stats_loop(agent: DQNAgent, bots: list[LearningBot], interval: float) -> None:
    while True:
        await asyncio.sleep(interval)
        eps_done = sum(b.episodes_finished for b in bots)
        # Rolling window across the swarm — this is the line to watch for
        # learning. score_mean rising over time is the only honest signal.
        n_win, score_mean, score_max, reward_mean, reward_max = agent.episode_window()

        # CPU-budget watchdog: aggregate per-bot timing snapshots into a
        # single swarm-wide line. Reads & resets the counters on each bot.
        snaps = [b.timing_snapshot() for b in bots]
        total_ticks = sum(s["tick_count"] for s in snaps)
        total_overruns = sum(s["overrun_count"] for s in snaps)
        total_work_s = sum(s["work_sum_s"] for s in snaps)
        max_work_s = max((s["max_work_s"] for s in snaps), default=0.0)
        # Per-bot realised Hz: total ticks / interval / n_bots. The user
        # configured `--control-hz`; if realised << configured the loop
        # is overrunning and the env_step rate is below what was asked.
        realised_hz_per_bot = total_ticks / max(1e-6, interval) / max(1, len(bots))
        configured_hz = bots[0].control_hz if bots else 0.0
        overrun_pct = 100.0 * total_overruns / max(1, total_ticks)
        avg_work_ms = 1000.0 * total_work_s / max(1, total_ticks)
        max_work_ms = 1000.0 * max_work_s

        line = (
            f"[stats] env_steps={agent.env_steps:>6}  train_steps={agent.train_steps:>6}  "
            f"eps={agent.epsilon():.3f}  loss={agent.last_loss:.3f}  "
            f"episodes={eps_done:>4}  "
        )
        if n_win > 0:
            line += (
                f"win{n_win}: ep_score(mean/max)={score_mean:.1f}/{score_max:.0f}  "
                f"ep_R(mean/max)={reward_mean:+.1f}/{reward_max:+.1f}"
            )
        else:
            # No deaths yet — fall back to live counters so something prints.
            live_score_mean = statistics.mean(
                int(b._my_player().get("score", 0)) if b._my_player() else 0
                for b in bots
            )
            line += f"live_score_mean={live_score_mean:.1f} (no episodes finished yet)"
        # Append the watchdog readings. `hz` is the realised tick rate
        # *averaged across bots*; if it's noticeably below configured,
        # the Python loop is the bottleneck. `overrun%` is the fraction
        # of ticks where the work itself exceeded the per-tick budget.
        line += (
            f"  | hz={realised_hz_per_bot:.1f}/{configured_hz:g}  "
            f"overrun={overrun_pct:.0f}%  avg_work={avg_work_ms:.1f}ms  "
            f"max_work={max_work_ms:.1f}ms"
        )
        print(line, flush=True)


async def _amain(args: argparse.Namespace) -> None:
    agent = DQNAgent(
        lr=args.lr,
        gamma=args.gamma,
        eps_decay_steps=args.eps_decay,
        warmup=args.warmup,
        device=args.device,
        checkpoint_path=args.checkpoint,
    )
    # Spin up the background trainer thread so gradient updates run in
    # parallel with the asyncio control loop. Without this, every
    # `_train_step` blocks the loop for several ms — at high control_hz
    # that silently dropped the realised tick rate (see the watchdog).
    agent.start_trainer()

    bots = [
        LearningBot(
            _make_name(i),
            agent=agent,
            url=args.url,
            control_hz=args.control_hz,
            action_repeat=args.action_repeat,
        )
        for i in range(args.count)
    ]
    tasks = []
    for b in bots:
        tasks.append(asyncio.create_task(b.run_forever()))
        if args.stagger > 0:
            await asyncio.sleep(args.stagger)
    stats = asyncio.create_task(_stats_loop(agent, bots, args.stats_interval))
    print(f"[run] spawned {args.count} learning bot(s) against {args.url}; ctrl-c to stop and save")
    try:
        await asyncio.gather(*tasks, stats)
    except asyncio.CancelledError:
        pass
    finally:
        # Stop the trainer thread first so it doesn't race with the final
        # save(); then save under the agent's own sync lock.
        agent.stop_trainer()
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
    args = p.parse_args()
    if args.checkpoint == "":
        args.checkpoint = None

    try:
        asyncio.run(_amain(args))
    except KeyboardInterrupt:
        print("\n[run] shutting down")
    return 0


if __name__ == "__main__":
    sys.exit(main())
