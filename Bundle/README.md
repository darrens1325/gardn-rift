# Bundle build

A single-`.wasm` build of gardn that fuses the Client, Server, Bots, and
all map assets into one self-contained `gardn-bundle.js` (which embeds the
`.wasm` via `-sSINGLE_FILE=1`). No HTTP, no Node, no real WebSocket — the
client talks to the server through an in-memory bridge, and bots run their
DQN policy in the browser via [onnxruntime-web].

## Quick start

```sh
# 1. (one-time) Export the trained PyTorch checkpoint to ONNX.
python Bots/export_onnx.py
# → writes Bots/model.onnx

# 2. Build the bundle. Requires Emscripten in $PATH (or EMSDK set).
cd Bundle && mkdir -p build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
# → writes build/gardn-bundle.js (single file; .wasm is base64 inside)

# 3. Run.
cp ../../Bots/model.onnx .
cp ../index.html .
python -m http.server 8000
open http://localhost:8000/
```

Open with `?bots=N` to spawn N bots (default 4). Set to 0 to play solo
against the server-side mob AI.

## What's in the bundle

* **Client**: every `Client/**.cc` plus its share of `Shared/`, compiled
  with `-DCLIENTSIDE=1 -DGARDN_BUNDLE=1` and wrapped in `namespace
  gardn::client`. `Float = LerpFloat` on this side.
* **Server**: every `Server/**.cc` (minus `Native.cc`, plus `Wasm.cc` with
  HTTP/Node-ws blocks `#ifdef`-out) plus its share of `Shared/`, compiled
  with `-DSERVERSIDE=1 -DWASM_SERVER=1 -DGARDN_BUNDLE=1` and wrapped in
  `namespace gardn::server`. `Float = float`.
* **Bots**: server-side C++ in [BotDriver.cc](BotDriver.cc) that computes
  observations from the live `Simulation` and applies actions via
  `Client::on_message`. Inference runs in JS through onnxruntime-web,
  loading `model.onnx` exported from the existing PyTorch checkpoint.
* **Assets**: `Map/main/main.tmj` and the entire `tiles/` directory are
  baked in via `--embed-file`, so `TiledMap.cc`'s `std::ifstream` calls
  work unchanged against Emscripten's MEMFS.
* **Bridge**: [Bridge.cc](Bridge.cc) defines the C ABI surface; the JS
  side ([index.html](index.html)) routes bytes between the two
  namespaces' linear-memory regions.

## Architecture

The two sides cannot share a translation unit: `Float` is a different
type on each (`LerpFloat` vs `float`), and `class Entity` therefore has a
different layout per side. The fix is to put each side in its own C++
namespace so duplicate definitions (`Entity::take_damage`, `frand`, etc.)
don't collide at link time.

[wrap_sources.py](wrap_sources.py) does this mechanically. For each
`.cc` file it emits a tiny generated TU:

```cpp
#include "Bundle/Prologue.hh"          // std headers at GLOBAL scope
namespace gardn::client {              // or gardn::server
#include "<absolute path to original>"
}
```

The prologue pulls every system header used anywhere in the tree into
`::std` *before* the namespace opens. Any `#include <vector>` inside a
wrapped TU then hits a `#pragma once` guard and becomes a no-op, so
`std::vector` stays in `::std` — not `gardn::client::std::vector`.

## Bridging

The standalone client builds a real `WebSocket`; the bundle replaces that
with two JS callbacks (see `Client/Socket.cc` and `Server/Wasm.cc` under
`#ifdef GARDN_BUNDLE`):

* **Client → Server**: `Socket::send` calls
  `Module._gardn_send_from_client(ptr, len)`. The harness copies bytes
  from the client's `HEAPU8` into the server's incoming buffer and
  invokes `_server_on_message(0, len)`. ws_id 0 = the local player.
* **Server → Client**: `WebSocket::send` calls
  `Module._gardn_send_from_server(ws_id, ptr, len)`. The harness routes
  ws_id 0 back to the client's `INCOMING_PACKET` and invokes
  `_client_on_message(1, len)`. Other ws_ids are bots — see below.

## Bot driver — MVP and porting status

The bot inference path is end-to-end:

1. `_bot_make_obs(ws_id, out_ptr)` (C++) writes a `[1, 89]` float vector.
2. JS wraps it in an `ort.Tensor`, runs `session.run({ state: tensor })`.
3. JS argmaxes the resulting logits, calls `_bot_apply_action(ws_id, a)`.
4. C++ builds a `Serverbound::kClientInput` / `kPetalSwap` / `kPetalDelete`
   packet and feeds it back into `Client::on_message` as if the bot were
   a real WebSocket client.

**The observation encoder is currently a stub.** It populates only the
first 13 dimensions (`BASE_STATE`: self HP + 3 nearest hostiles × 4
features). The remaining 76 are zero. The Python bot trains on a much
richer observation; the matching ports are:

| Slice                 | Dims | Status | Source                            |
| --------------------- | ---- | ------ | --------------------------------- |
| BASE_STATE            | 13   | done   | `BotDriver.cc`                    |
| Loadout rank          | 16   | TODO   | `protocol.py:petal_rank`          |
| Peer comm             | 12   | TODO   | `agent.py:read_peers`             |
| Loadout type          | 16   | TODO   | `protocol.py:petal_type_norm`     |
| Drops                 | 12   | TODO   | `bot.py` (nearest-K drops scan)   |
| Loadout burst         | 16   | TODO   | `protocol.py:petal_burst_norm`    |
| Wall rays             |  4   | TODO   | `wall_map.py:wall_ray_features`   |

Until these are ported the bots play with a degraded policy: argmax over
logits computed from mostly-zero features. They still issue legal
movement and inventory actions, just not as well as `run.py` does.

## Known limitations

* `Server/Wasm.cc` still pulls in some Node-specific code paths under
  `#ifndef GARDN_BUNDLE`. The bundle build never compiles those — they're
  preprocessed away — but they remain present in the source.
* `std::thread` references in `Server/Game.hh` (the
  `stdin_wave_tick_override` atomic) are harmless: the bundle never spawns
  threads, and `std::atomic` works fine single-threaded.
* The HTML harness expects `model.onnx` and `gardn-bundle.js` to sit in
  the same directory as `index.html` over HTTP. The `.wasm` is embedded
  in the `.js` (no separate file). For a *truly* one-file deliverable
  you can inline `gardn-bundle.js` as a `<script>` and the model as a
  base64 `data:` URL, but the harness keeps them separate by default for
  iteration speed.
