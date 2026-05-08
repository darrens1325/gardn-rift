"""Per-bot memory.

Two scopes:
  - PersistentMemory: JSON file at Bots/state/<bot-name>.json. Survives across
    process restarts. Carries lifetime stats and identity-level info: total
    episodes, cumulative score, best run, who's killed this bot the most.
  - EpisodicMemory: in-RAM, lives only for one episode (one player life).
    Holds the bot's recent observations — who damaged us, what we killed,
    who we last saw. Reset on death.

The DQN doesn't directly read from these. They drive chat triggers and bot-
to-bot bookkeeping; future work could fold them into the observation vector.
"""

from __future__ import annotations

import json
import os
import re
import tempfile
import time
from collections import Counter, deque
from typing import Any

# Where per-bot JSON state lives. Bots/state/<safe_name>.json. Created lazily.
DEFAULT_STATE_DIR = os.path.join(os.path.dirname(__file__), "state")

# Filename-safe replacement for arbitrary bot names. Names can contain unicode,
# spaces, etc. This keeps things to [-A-Za-z0-9_] and adds a length cap.
_SAFE_NAME = re.compile(r"[^A-Za-z0-9_-]+")


def _safe_filename(name: str) -> str:
    s = _SAFE_NAME.sub("_", name).strip("_")
    if not s:
        s = "_anon"
    return s[:48]


class PersistentMemory:
    """JSON-backed per-bot lifetime stats. Loaded once on construction; saved
    explicitly via `save()` (typically after each finished episode)."""

    SCHEMA_VERSION = 1

    def __init__(self, name: str, state_dir: str | None = None) -> None:
        self.name = name
        self.dir = state_dir or DEFAULT_STATE_DIR
        os.makedirs(self.dir, exist_ok=True)
        self.path = os.path.join(self.dir, _safe_filename(name) + ".json")
        self._data: dict[str, Any] = self._default()
        self.load()

    @staticmethod
    def _default() -> dict[str, Any]:
        return {
            "schema": PersistentMemory.SCHEMA_VERSION,
            "name": "",
            "first_seen": 0.0,                # epoch seconds — set on first save
            "last_seen": 0.0,                 # epoch seconds — updated each save
            "episodes": 0,                    # total deaths counted
            "score_total": 0,                 # sum of score-at-death across episodes
            "score_best": 0,                  # best single-episode score
            "best_loadout": [],               # primary loadout that earned score_best
            # Map<sender_name, count> of who killed us most (when we can identify).
            "killed_by": {},
            # Petals we've ever held in our primary loadout, with how many
            # episodes we've equipped them. Useful for reading lifetime taste.
            "loadout_history": {},
        }

    def load(self) -> None:
        try:
            with open(self.path, "r", encoding="utf-8") as f:
                loaded = json.load(f)
            if not isinstance(loaded, dict):
                return
            if loaded.get("schema") != self.SCHEMA_VERSION:
                # Future: migrate old schemas here. For now, keep defaults.
                return
            self._data.update(loaded)
            # Force the canonical name even if the file was renamed.
            self._data["name"] = self.name
        except FileNotFoundError:
            pass
        except (json.JSONDecodeError, OSError):
            # Don't blow up on corrupt state — just start fresh.
            pass

    def save(self) -> None:
        self._data["name"] = self.name
        now = time.time()
        if not self._data.get("first_seen"):
            self._data["first_seen"] = now
        self._data["last_seen"] = now
        # Atomic write: temp file in same dir, rename. Avoids a half-written
        # JSON if the process dies mid-flush.
        tmp = tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=self.dir, delete=False, suffix=".tmp"
        )
        try:
            json.dump(self._data, tmp, indent=2)
            tmp.flush()
            os.fsync(tmp.fileno())
            tmp.close()
            os.replace(tmp.name, self.path)
        except Exception:
            try:
                os.unlink(tmp.name)
            except OSError:
                pass
            raise

    # ---- accessors / mutators ---------------------------------------------

    @property
    def episodes(self) -> int:
        return int(self._data.get("episodes", 0))

    @property
    def score_total(self) -> int:
        return int(self._data.get("score_total", 0))

    @property
    def score_best(self) -> int:
        return int(self._data.get("score_best", 0))

    @property
    def best_loadout(self) -> list[int]:
        return list(self._data.get("best_loadout", []))

    @property
    def avg_score(self) -> float:
        eps = self.episodes
        return self.score_total / eps if eps else 0.0

    def killed_by_top(self, n: int = 3) -> list[tuple[str, int]]:
        kb = self._data.get("killed_by", {}) or {}
        return sorted(kb.items(), key=lambda x: -x[1])[:n]

    def record_episode(
        self,
        score: int,
        primary_loadout: list[int],
        killed_by_name: str | None = None,
    ) -> None:
        """Update lifetime stats after a death. Caller is responsible for save()."""
        d = self._data
        d["episodes"] = int(d.get("episodes", 0)) + 1
        d["score_total"] = int(d.get("score_total", 0)) + int(score)
        if int(score) > int(d.get("score_best", 0)):
            d["score_best"] = int(score)
            d["best_loadout"] = [int(x) for x in primary_loadout]
        # Loadout taste: count distinct petal types we equipped this run.
        hist = d.setdefault("loadout_history", {})
        for pid in set(primary_loadout):
            key = str(int(pid))
            hist[key] = int(hist.get(key, 0)) + 1
        if killed_by_name:
            kb = d.setdefault("killed_by", {})
            kb[killed_by_name] = int(kb.get(killed_by_name, 0)) + 1


class EpisodicMemory:
    """Per-life scratch state. Reset on `clear()` (death/respawn). The DQN's
    transitions live in the agent's replay buffer; this is for higher-level
    bookkeeping the chat triggers and inventory diagnostics consume."""

    def __init__(self) -> None:
        # Recent chat lines from anyone (sender_name, color, text, recv_time).
        # Bounded — older lines fall off.
        self.chat_log: deque[tuple[str, int, str, float]] = deque(maxlen=20)
        # Score deltas observed this life. Used to detect "big kill" events
        # without having to introspect Damage.cc — a sudden +large delta is
        # likely a player kill (mob kills are usually +1..+5).
        self.score_deltas: deque[tuple[float, int]] = deque(maxlen=64)
        # Who hit us recently this life: counts of damager EntityIDs.
        self.damagers: Counter = Counter()
        # When we last sent chat (wall-clock seconds). Cooled down beyond
        # the server's per-tick rate-limit so we don't drown the channel.
        self.last_chat_sent: float = 0.0
        # Tag of the last "low-HP shout" so we don't repeat it within an episode.
        self.shouted_low_hp: bool = False
        # Snapshot of the player's primary loadout we last saw. Captured at
        # death so we can credit best_loadout correctly.
        self.last_primary_loadout: list[int] = []
        # Latest structured-chat payload we've decoded from each peer, by
        # message kind. Shape: peer_state[sender_name][kind] = (recv_time, payload).
        # Survives across our own deaths because peer state is a property of
        # the channel, not of our life.
        self.peer_state: dict[str, dict[str, tuple[float, dict]]] = {}
        # Wall-clock timestamps of our own last-sent data-chat messages by
        # kind. Used to throttle each kind independently — e.g. we want to
        # broadcast position frequently but kills only when they happen.
        self.last_data_sent: dict[str, float] = {}

    def note_chat(self, sender_name: str, color: int, text: str) -> None:
        self.chat_log.append((sender_name, color, text, time.time()))

    def note_score_delta(self, delta: int) -> None:
        if delta > 0:
            self.score_deltas.append((time.time(), int(delta)))

    def biggest_recent_score_delta(self) -> int:
        return max((d for _, d in self.score_deltas), default=0)

    def clear(self) -> None:
        # Don't clear chat_log — chat is a global stream, not per-life.
        self.score_deltas.clear()
        self.damagers.clear()
        self.shouted_low_hp = False
        self.last_primary_loadout = []
