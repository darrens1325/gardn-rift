"""Side-by-side debug harness for the bundle's bot policy.

Open the bundle in a browser, watch DevTools for the `[obs-dump]` lines
the JS prints once a second. Each pair gives one bot's argmax action and
its 89-float observation vector. Copy the `obs=[...]` payload here and
this script feeds the same vector through `agent.act(greedy=True)`
(pure argmax of `q_inference`) — the same network the ONNX export was
built from. If the bundle's argmax and this script's argmax agree, the
bundle is faithful to the trained policy; if they disagree, the model
output differs between ORT-web and PyTorch CPU on the same input
(usually a WebGPU precision or kernel divergence).

Usage:
  # paste the JS array as-is, with or without the surrounding `obs=`
  python Bots/inspect_obs.py '[0.95,0.12,-0.34,...]'

  # or via stdin (easier for very long lines):
  pbpaste | python Bots/inspect_obs.py -
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import torch  # noqa: E402

from agent import (  # noqa: E402
    DQNAgent,
    NUM_ACTIONS,
    NUM_MOVEMENT_ACTIONS,
    STATE_DIM,
)

# Same direction mapping as Bundle/index.html::actionName (and agent.py
# _DIRECTIONS). gardn uses screen-space coordinates with +y pointing
# down, so k=1 = (0.707, 0.707) is SE not NE.
_DIR_NAMES = ["stay", "E", "SE", "S", "SW", "W", "NW", "N", "NE"]


def action_name(a: int) -> str:
    if a < NUM_MOVEMENT_ACTIONS:
        prefix = "atk_" if a < 9 else "def_"
        return prefix + _DIR_NAMES[a % 9]
    rel = a - NUM_MOVEMENT_ACTIONS
    if rel < 8:
        return f"swap[{rel}<->{rel + 8}]"
    return f"del[{rel - 8}]"


def parse_obs(raw: str) -> list[float]:
    # Tolerate the leading `obs=` JS prints, with or without spaces.
    s = raw.strip()
    if s.startswith("obs="):
        s = s[4:]
    # Tolerate comma-separated bare numbers (no brackets) too.
    if not s.startswith("["):
        s = "[" + s + "]"
    return [float(x) for x in json.loads(s)]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("obs", nargs="?", default=None,
                    help="89-float observation as JSON list, or '-' to read stdin")
    ap.add_argument("--checkpoint",
                    default=os.path.join(os.path.dirname(__file__), "model.pt.best"),
                    help="QNet checkpoint to load (default: Bots/model.pt.best)")
    args = ap.parse_args()

    if args.obs is None or args.obs == "-":
        raw = sys.stdin.read()
    else:
        raw = args.obs
    obs = parse_obs(raw)
    if len(obs) != STATE_DIM:
        sys.exit(f"obs has {len(obs)} floats, expected {STATE_DIM}")

    agent = DQNAgent(checkpoint_path=args.checkpoint)
    with torch.no_grad():
        s = torch.as_tensor(obs, dtype=torch.float32).unsqueeze(0)
        logits = agent.q_inference(s)[0]
    ranked = sorted(
        ((int(i), float(v)) for i, v in enumerate(logits.tolist())),
        key=lambda t: -t[1],
    )
    argmax = ranked[0][0]
    print(f"py argmax = {argmax} ({action_name(argmax)})")
    print("py top-5 logits:")
    for a, v in ranked[:5]:
        print(f"  {a:3d}  {action_name(a):<14}  {v:+.4f}")


if __name__ == "__main__":
    main()
