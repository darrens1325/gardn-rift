"""Export the trained DQN QNet to ONNX for the bundle build.

The bundle (Bundle/index.html) loads `model.onnx` via onnxruntime-web and
runs inference in the browser. The bundle's C++ bot driver computes the
89-dim observation vector and feeds it in under input name "state"; the
ONNX graph returns logits of length NUM_ACTIONS (42).

Usage:
    python Bots/export_onnx.py [--checkpoint Bots/model.pt] [--out Bots/model.onnx]

Notes:
  - The exported graph is the *inference* network (`agent.q`), not the
    target net. We snapshot from a freshly-constructed DQNAgent so the
    QNet's architecture is whatever agent.py currently declares — STATE_DIM
    and NUM_ACTIONS must match Bundle/BotDriver.cc (BOT_OBS_DIM=89,
    BOT_NUM_ACTIONS=42).
  - We pad-load the checkpoint to tolerate the same architecture drift
    that run.py handles (see _pad_load_state_dict in agent.py).
"""

from __future__ import annotations

import argparse
import os
import sys

# Make Bots/ imports work when run from the repo root.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import torch

from agent import DQNAgent, STATE_DIM, NUM_ACTIONS


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--checkpoint", default=os.path.join(os.path.dirname(__file__), "model.pt"))
    ap.add_argument("--out",        default=os.path.join(os.path.dirname(__file__), "model.onnx"))
    ap.add_argument("--opset",      type=int, default=17,
                    help="ONNX opset (onnxruntime-web 1.18 supports up to 20)")
    args = ap.parse_args()

    print(f"[export] STATE_DIM={STATE_DIM} NUM_ACTIONS={NUM_ACTIONS}")
    if not os.path.exists(args.checkpoint):
        print(f"[export] no checkpoint at {args.checkpoint}; exporting a random-init network",
              file=sys.stderr)
        agent = DQNAgent(checkpoint_path=None)
    else:
        # DQNAgent.__init__ pad-loads automatically; we go through it so the
        # behavior matches run.py exactly.
        agent = DQNAgent(checkpoint_path=args.checkpoint)

    # Pull the live Q-network. Eval mode + no_grad ensures the exported
    # graph has no training-only ops (e.g. Dropout, BatchNorm running stats).
    net = agent.q
    net.eval()

    dummy = torch.zeros(1, STATE_DIM, dtype=torch.float32)
    # dynamo=False forces the legacy tracer-based exporter. The new
    # dynamo path (default in PyTorch 2.5+) emits ops that onnxruntime-web
    # 1.18 hasn't shipped kernels for — the bundle's session.run() fails
    # with `TypeError: Lt[m] is not a function`. The legacy exporter
    # produces only the classic Gemm/Relu/MatMul set, which ORT-web
    # handles fine for an MLP this simple.
    export_kwargs = dict(
        export_params=True,
        opset_version=args.opset,
        do_constant_folding=True,
        input_names=["state"],
        output_names=["q_values"],
        dynamic_axes={"state": {0: "batch"}, "q_values": {0: "batch"}},
    )
    try:
        torch.onnx.export(net, dummy, args.out, dynamo=False, **export_kwargs)
    except TypeError:
        # PyTorch <2.5 doesn't have the `dynamo` kwarg — fall back to its
        # default (legacy-tracer) path, which is what we want anyway.
        torch.onnx.export(net, dummy, args.out, **export_kwargs)
    print(f"[export] wrote {args.out}  ({os.path.getsize(args.out)} bytes)")


if __name__ == "__main__":
    main()
