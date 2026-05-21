// Server-side bot driver: builds a 100-dim observation per bot from the
// live Simulation and applies a chosen action by routing a kClientInput
// (or kPetalSwap/kPetalDelete) packet through Client::on_message — exactly
// what a real WebSocket client would do.
//
// This file is one of the SERVER wrapped sources: the build wraps it in
// `namespace gardn::server`. Forward declarations in Bundle/Bridge.cc refer
// to `gardn::server::bot_make_obs_impl` etc.
//
// MVP scope: matches agent.py / bot.py's STATE_DIM=100 and NUM_ACTIONS=42
// layout so the ONNX-exported QNet runs unmodified. Movement actions
// (0..17) are decoded into (vx, vy, input_flags) and sent as kClientInput.
// Inventory actions (18..41) are stubbed. Spawn-on-first-tick is handled
// automatically: if the bot has no live flower we issue a kClientSpawn
// with a procedural name.

#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>
#include <Server/TiledMap.hh>
#include <Shared/Binary.hh>
#include <Shared/Config.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/Vector.hh>

#include <algorithm>
#include <cmath>
#include <cstring>

// Mirror agent.py — kept in sync with STATE_DIM and NUM_ACTIONS there.
static constexpr int BOT_OBS_DIM = 100;
static constexpr int BOT_NUM_ACTIONS = 42;
static constexpr int NUM_MOVEMENT_ACTIONS = 18;
static constexpr int NUM_DIRECTIONS = 9;
static constexpr float OBS_SCALE = 1500.0f;
static constexpr float MOVE_MAG = 260.0f;
static constexpr uint8_t INPUT_ATTACK = 1;
static constexpr uint8_t INPUT_DEFEND = 2;

// agent.py _DIRECTIONS: stay + 8 compass points.
static const float DIR_TABLE[NUM_DIRECTIONS][2] = {
    { 0.0f,  0.0f},
    { 1.0f,  0.0f},
    { 0.70710678f,  0.70710678f},
    { 0.0f,  1.0f},
    {-0.70710678f,  0.70710678f},
    {-1.0f,  0.0f},
    {-0.70710678f, -0.70710678f},
    { 0.0f, -1.0f},
    { 0.70710678f, -0.70710678f},
};

extern std::unordered_map<int, WebSocket *> WS_MAP;  // defined in Server/Wasm.cc

// The bundle's URL-driven map override. Read once from
// window.location.search at first call and cached for the rest of the
// process; bundled bots send this verbatim as the kClientSpawn map_path
// so they spawn alongside the human player on whatever map they picked.
// Server-side validation (Server/Client.cc::is_safe_user_map_path) is
// authoritative — we just forward the raw string and let the server
// silently fall back if it's empty/malformed.
static std::string const &bot_spawn_map_path() {
    static std::string cached = []() -> std::string {
        char *ptr = (char *) EM_ASM_PTR({
            try {
                const params = new URLSearchParams(window.location.search);
                const v = params.get("spawn") || "";
                const arr = new TextEncoder().encode(v);
                const p = Module["_malloc"](arr.length + 1);
                HEAPU8.set(arr, p);
                HEAPU8[p + arr.length] = 0;
                return p;
            } catch (e) {
                const p = Module["_malloc"](1);
                HEAPU8[p] = 0;
                return p;
            }
        });
        std::string out{ptr ? ptr : ""};
        if (ptr) std::free(ptr);
        return out;
    }();
    return cached;
}

// Per-bot "ready for next spawn" gate. Mirrors bot.py::_can_respawn —
// True at connect (so the bot's first life starts immediately), False
// after each spawn, re-armed True when the server's wave_tick wraps
// back to 0 (the only thing that resets is end_round in Game.cc, so
// the wrap is a reliable round-end edge). Without this gate the
// bundle's bots respawn every tick they're dead, which means the
// last-man-standing wave system can never resolve — there's always
// somebody alive.
static std::unordered_map<int, bool> g_bot_can_respawn;
static uint32_t g_prev_wave_tick = 0;

// Called once per bot_apply_action. The first bot in a tick detects
// the wave_tick wrap-to-0 edge and re-arms every bot's gate; the rest
// see `now == prev` and no-op.
static void update_round_gate() {
    uint32_t now = Server::game.wave_tick;
    if (now < g_prev_wave_tick) {
        // Round just ended (end_round() set wave_tick = 0). Re-arm
        // every tracked bot.
        for (auto &kv : g_bot_can_respawn) kv.second = true;
    }
    g_prev_wave_tick = now;
}

// Per-bot last action — the in-process equivalent of Bots/agent.py's
// peer-message blackboard. Other bots in the same bundle read these
// when filling their peer_comm observation slots so the model gets the
// same "what did my swarm-mates just decide" signal it was trained on.
// Without this, the 12 peer slots are all zero in the bundle, which
// puts the trained policy out of distribution — peer features were
// almost always non-zero during training (the swarm shares an agent),
// so the model interpreted the zero-vector as a meaningful state and
// the bundled bots adopted a slightly different action distribution.
static std::unordered_map<int, int> g_bot_last_action;

// Mirror of bot.py / protocol.py constants. Kept in sync by hand.
static constexpr int   K_PEERS          = 2;
static constexpr int   COMM_PER_PEER    = 6;
static constexpr int   K_DROPS          = 3;
static constexpr int   DROP_FEAT_PER    = 4;
static constexpr float MAX_RARITY_RANK  = 6.0f;   // RARITY_UNIQUE
static constexpr float PETAL_MAX_BURST  = 150.0f;
static constexpr float WALL_RAY_CAP     = 2000.0f;
static constexpr int NUM_PETAL_TYPES_INC_NONE = 6;   // matches protocol.py

// State-vector offsets (cumulative, append-only — must match agent.py).
// [HP, hostile×12, loadout_rank×16, peer_comm×12, loadout_type×16,
//  drops×12, loadout_burst×16, wall_rays×4, warps×8, minimap×3]
static constexpr int OFF_HP             = 0;
static constexpr int OFF_HOSTILE        = 1;
static constexpr int OFF_LOADOUT_RANK   = 13;
static constexpr int OFF_PEER_COMM      = 29;
static constexpr int OFF_LOADOUT_TYPE   = 41;
static constexpr int OFF_DROPS          = 57;
static constexpr int OFF_LOADOUT_BURST  = 69;
static constexpr int OFF_WALL_RAYS      = 85;
static constexpr int OFF_WARPS          = 89;
static constexpr int OFF_MINIMAP        = 97;

// Warp/minimap feature shape — mirrors Bots/wall_map.py:
//   K_WARPS × (rel_dx_norm, rel_dy_norm, dist_norm, inside_trigger_flag)
//   then (x_norm, y_norm, no_map_flag).
static constexpr int K_WARPS            = 2;
static constexpr int WARP_FEAT_PER_SLOT = 4;

// Mirror of protocol.py PETAL_TYPE_*: 0=NONE 1=DAMAGE 2=TANK 3=HEAL 4=POISON 5=UTILITY.
enum PetalTypeCat { PT_NONE=0, PT_DAMAGE=1, PT_TANK=2, PT_HEAL=3, PT_POISON=4, PT_UTILITY=5 };

// Classify a petal by inspecting its PETAL_DATA entry. Mirrors the
// hand-authored protocol.py PETAL_TYPE table:
//   1. attributes.constant_heal > 0 or attributes.burst_heal > 0 → HEAL
//   2. attributes.poison_damage.damage > 0 → POISON
//   3. names matching the TANK / DAMAGE lists below → that category
//   4. everything else → UTILITY (flag-style petals: Antennae, Bubble,
//      Missile, Web, Wing, Faster, Egg variants, Stick, Salt, ThirdEye,
//      Observer, Lotus, Cutter, YinYang, Yggdrasil, Square, Root, …)
static int classify_petal_type(PetalID::T id) {
    if (id == PetalID::kNone || id >= PetalID::kNumPetals) return PT_NONE;
    PetalData const &d = PETAL_DATA[id];
    if (d.attributes.constant_heal > 0 || d.attributes.burst_heal > 0) return PT_HEAL;
    if (d.attributes.poison_damage.damage > 0) return PT_POISON;
    char const *n = d.name;
    // TANK: chunky high-HP defensive petals.
    if (!std::strcmp(n, "Heavy") || !std::strcmp(n, "Rock") || !std::strcmp(n, "Cactus")
        || !std::strcmp(n, "Tricac") || !std::strcmp(n, "Heaviest") || !std::strcmp(n, "Moon")
        || !std::strcmp(n, "Bone") || !std::strcmp(n, "Corn")) return PT_TANK;
    // DAMAGE: offensive petals with no special role.
    if (!std::strcmp(n, "Basic") || !std::strcmp(n, "Fast") || !std::strcmp(n, "Stinger")
        || !std::strcmp(n, "Twin") || !std::strcmp(n, "Triplet") || !std::strcmp(n, "Peas")
        || !std::strcmp(n, "Sand") || !std::strcmp(n, "Rice") || !std::strcmp(n, "Square")
        || !std::strcmp(n, "Tringer")) return PT_DAMAGE;
    return PT_UTILITY;
}

// 4 cardinal-direction wall-distance rays. Mirrors Bots/wall_map.py —
// the cardinal-ray AABB check collapses to a 1-D containment test:
// vertical rays only care about walls whose x-span contains px, etc.
// Reads TiledMap::collision_rects + collision_polys (the latter via
// their bounding boxes — exact polygon vs ray is overkill for a
// cardinal sensor). Output order matches WALL_FEAT layout in
// wall_map.py: [N, E, S, W].
static void wall_ray_features(float px, float py, float *out) {
    float north = py;
    float south = (float)ARENA_HEIGHT - py;
    float west  = px;
    float east  = (float)ARENA_WIDTH  - px;
    if (north < 0) north = 0;
    if (south < 0) south = 0;
    if (west  < 0) west  = 0;
    if (east  < 0) east  = 0;
    auto consider_rect = [&](float rx, float ry, float rw, float rh) {
        if (rx <= px && px <= rx + rw) {
            if (ry + rh <= py) {
                float d = py - (ry + rh);
                if (d < north) north = d;
            } else if (ry >= py) {
                float d = ry - py;
                if (d < south) south = d;
            }
        }
        if (ry <= py && py <= ry + rh) {
            if (rx + rw <= px) {
                float d = px - (rx + rw);
                if (d < west) west = d;
            } else if (rx >= px) {
                float d = rx - px;
                if (d < east) east = d;
            }
        }
    };
    for (auto const &r : TiledMap::collision_rects)
        consider_rect(r.x, r.y, r.w, r.h);
    for (auto const &p : TiledMap::collision_polys)
        consider_rect(p.min_x, p.min_y, p.max_x - p.min_x, p.max_y - p.min_y);
    out[0] = std::min(north / WALL_RAY_CAP, 1.0f);
    out[1] = std::min(east  / WALL_RAY_CAP, 1.0f);
    out[2] = std::min(south / WALL_RAY_CAP, 1.0f);
    out[3] = std::min(west  / WALL_RAY_CAP, 1.0f);
}

static Client *client_for_ws(int ws_id) {
    auto it = WS_MAP.find(ws_id);
    if (it == WS_MAP.end() || it->second == nullptr) return nullptr;
    return it->second->getUserData();
}

// Encode a 100-dim observation from the current sim state for the bot at
// ws_id. Out-of-bound / dead-bot calls just zero-fill so the model
// produces deterministic output (typically biased toward action 0 = stay).
//
// Layout matches agent.py exactly:
//   [0..0]    self HP                                         (1)
//   [1..12]   3 nearest hostiles × (dx,dy,is_player,hp)       (12)
//   [13..28]  loadout slots × rank_norm                        (16)
//   [29..40]  peer comm (left zero — no in-bundle blackboard)  (12)
//   [41..56]  loadout slots × type_norm                        (16)
//   [57..68]  3 nearest drops × (dx,dy,rank_norm,type_norm)    (12)
//   [69..84]  loadout slots × burst_norm                       (16)
//   [85..88]  4 cardinal wall-ray distances (N,E,S,W)          (4)
//   [89..96]  2 nearest warps × (dx,dy,dist,inside)            (8)
//   [97..99]  minimap: (x_norm, y_norm, no_map_flag)           (3)
void bot_make_obs_impl(int ws_id, float *out) {
    for (int i = 0; i < BOT_OBS_DIM; ++i) out[i] = 0.0f;
    Client *client = client_for_ws(ws_id);
    if (client == nullptr || !client->verified) return;
    Simulation *sim = &Server::game.simulation;
    if (!sim->ent_exists(client->camera)) return;
    Entity &camera = sim->get_ent(client->camera);
    if (!sim->ent_alive(camera.player)) return;
    Entity &me = sim->get_ent(camera.player);

    float mx = (float)me.x, my = (float)me.y;
    EntityID me_id = me.id;
    // Mirror bot.py: the team used for friend/foe is the *camera's* team,
    // not the player's. In current Spawn.cc they're equal because
    // alloc_player(sim, camera.team) is what sets player.team, but
    // bot.py reads `cam["team"]` so we do the same here for parity.
    EntityID me_team = camera.team;

    // [HP + 3 hostiles × 4]
    out[OFF_HP] = (float)me.health_ratio;
    struct Hostile { float d2, dx, dy, is_player, hp; };
    Hostile nearest[3] = { {1e30f,0,0,0,0}, {1e30f,0,0,0,0}, {1e30f,0,0,0,0} };

    // [3 nearest drops × 4]
    struct DropFeat { float d2, dx, dy; int drop_id; };
    DropFeat drops[K_DROPS] = { {1e30f,0,0,0}, {1e30f,0,0,0}, {1e30f,0,0,0} };

    // Critical: the Python bot's observation is built from `self.entities`,
    // which is populated from each kClientUpdate packet — and that packet
    // only carries entities within the camera's visible AABB (see
    // Server/Game.cc::_update_client). Hostiles outside that rectangle
    // are simply NOT in the Python bot's world view. Iterating
    // `sim->for_each_entity` here exposes the *entire* arena, so the
    // nearest-hostile slot picks up flowers/mobs at e.g. 5000 units
    // (dx/OBS_SCALE ≈ 3.3) — values the trained model never saw, since
    // the camera AABB caps any visible hostile near ±(960/fov+50,
    // 540/fov+50) ≈ ±(1117, 650). The model then picks pseudo-random
    // actions on those out-of-distribution inputs and the bots act
    // "sluggish / slow to react." Use the same spatial_hash.query the
    // server uses to assemble in_view, against the *camera* position
    // and view extents, so the observation set matches exactly.
    float cam_x = (float)camera.camera_x;
    float cam_y = (float)camera.camera_y;
    float view_w = 960.0f / (float)camera.fov + 50.0f;
    float view_h = 540.0f / (float)camera.fov + 50.0f;
    sim->spatial_hash.query(cam_x, cam_y, view_w, view_h, [&](Simulation *, Entity &e) {
        if (e.id == me_id) return;
        if (!e.has_component(kPhysics)) return;
        // Match bot.py: skip entities flagged for deletion this tick.
        // They still appear in the world until post_tick clears them,
        // but bot.py drops them via `ent.get("_pending_delete")`.
        // Without this filter the bundle observation can include a
        // just-killed hostile as the nearest target, perturbing the
        // model relative to its training-time inputs.
        if (e.pending_delete) return;
        float dx = (float)e.x - mx;
        float dy = (float)e.y - my;
        float d2 = dx*dx + dy*dy;
        if (e.has_component(kDrop)) {
            for (int k = 0; k < K_DROPS; ++k) {
                if (d2 < drops[k].d2) {
                    for (int j = K_DROPS - 1; j > k; --j) drops[j] = drops[j-1];
                    drops[k] = { d2, dx, dy, (int)e.drop_id };
                    break;
                }
            }
            return;
        }
        bool is_mob = e.has_component(kMob);
        bool is_flower = e.has_component(kFlower);
        if (!is_mob && !is_flower) return;
        if (e.team == me_team) return;
        if (!e.has_component(kHealth)) return;
        for (int k = 0; k < 3; ++k) {
            if (d2 < nearest[k].d2) {
                for (int j = 2; j > k; --j) nearest[j] = nearest[j-1];
                nearest[k] = { d2, dx, dy, is_flower ? 1.0f : 0.0f, (float)e.health_ratio };
                break;
            }
        }
    });
    for (int k = 0; k < 3; ++k) {
        int base = OFF_HOSTILE + k * 4;
        out[base + 0] = nearest[k].dx / OBS_SCALE;
        out[base + 1] = nearest[k].dy / OBS_SCALE;
        out[base + 2] = nearest[k].is_player;
        out[base + 3] = nearest[k].hp;
    }

    // Loadout features (rank / type / burst). One pass over the 16-slot
    // inventory; populates three parallel state-vector columns. Matches
    // bot.py::_loadout_features / _loadout_type_features /
    // _loadout_burst_features.
    for (int i = 0; i < 16; ++i) {
        PetalID::T pid = me.loadout_ids[i];
        if (pid == PetalID::kNone || pid >= PetalID::kNumPetals) continue;
        PetalData const &pd = PETAL_DATA[pid];
        // rank: (rarity + 1) / (MAX_RARITY_RANK + 1) → kCommon ≈ 0.14, kUnique = 1.0
        out[OFF_LOADOUT_RANK + i] = ((float)pd.rarity + 1.0f) / (MAX_RARITY_RANK + 1.0f);
        // type: classifier / (NUM_TYPES_INC_NONE - 1) so empty=0, utility=1.0
        int t = classify_petal_type(pid);
        out[OFF_LOADOUT_TYPE + i] = (float)t / (float)(NUM_PETAL_TYPES_INC_NONE - 1);
        // burst: damage × count, clamped to 1.0 by PETAL_MAX_BURST.
        float burst = (float)pd.damage * (float)pd.count;
        out[OFF_LOADOUT_BURST + i] = std::min(burst / PETAL_MAX_BURST, 1.0f);
    }

    // Drops: 3 nearest by squared distance, each as (dx, dy, rank, type).
    for (int k = 0; k < K_DROPS; ++k) {
        int base = OFF_DROPS + k * DROP_FEAT_PER;
        if (drops[k].d2 >= 1e29f) continue;  // empty slot — leave zeros
        out[base + 0] = drops[k].dx / OBS_SCALE;
        out[base + 1] = drops[k].dy / OBS_SCALE;
        int did = drops[k].drop_id;
        if (did > 0 && did < PetalID::kNumPetals) {
            PetalData const &pd = PETAL_DATA[did];
            out[base + 2] = ((float)pd.rarity + 1.0f) / (MAX_RARITY_RANK + 1.0f);
            out[base + 3] = (float)classify_petal_type((PetalID::T)did) / (float)(NUM_PETAL_TYPES_INC_NONE - 1);
        }
    }

    // Wall rays. 4 cardinal directions, clamped to WALL_RAY_CAP.
    wall_ray_features(mx, my, &out[OFF_WALL_RAYS]);

    // Warps (portals) + minimap (normalised global position). Mirrors
    // Bots/wall_map.py:warp_features and minimap_features so the bundled
    // bots see the same trailing observation columns the trained QNet
    // expects. The server's TiledMap::warps holds the current map's
    // warps (populated by TiledMap::load); we pick the K_WARPS nearest
    // by squared distance and fill (rel_dx, rel_dy, dist, inside_flag)
    // per slot.
    struct WarpFeat { float d2, dx, dy, radius; };
    WarpFeat near_warps[K_WARPS];
    for (int i = 0; i < K_WARPS; ++i) near_warps[i] = {1e30f, 0, 0, 0};
    for (auto const &w : TiledMap::warps) {
        float dx = (float)w.x - mx;
        float dy = (float)w.y - my;
        float d2 = dx * dx + dy * dy;
        for (int k = 0; k < K_WARPS; ++k) {
            if (d2 < near_warps[k].d2) {
                for (int j = K_WARPS - 1; j > k; --j) near_warps[j] = near_warps[j-1];
                near_warps[k] = {d2, dx, dy, (float)w.radius};
                break;
            }
        }
    }
    for (int k = 0; k < K_WARPS; ++k) {
        int base = OFF_WARPS + k * WARP_FEAT_PER_SLOT;
        if (near_warps[k].d2 >= 1e29f) continue;  // empty slot — zeros
        float dist = std::sqrt(near_warps[k].d2);
        out[base + 0] = near_warps[k].dx / OBS_SCALE;
        out[base + 1] = near_warps[k].dy / OBS_SCALE;
        out[base + 2] = std::min(dist / OBS_SCALE, 1.0f);
        out[base + 3] = dist <= near_warps[k].radius ? 1.0f : 0.0f;
    }

    // Minimap: normalised global position. ARENA_WIDTH/HEIGHT come from
    // the configure-time Shared/MapDimensions.hh header, so they reflect
    // the bundle's compiled-in map regardless of any runtime override.
    float aw = (float)ARENA_WIDTH;
    float ah = (float)ARENA_HEIGHT;
    if (aw > 0.0f && ah > 0.0f) {
        float xn = mx / aw;
        float yn = my / ah;
        if (xn < 0.0f) xn = 0.0f; else if (xn > 1.0f) xn = 1.0f;
        if (yn < 0.0f) yn = 0.0f; else if (yn > 1.0f) yn = 1.0f;
        out[OFF_MINIMAP + 0] = xn;
        out[OFF_MINIMAP + 1] = yn;
        out[OFF_MINIMAP + 2] = 0.0f;
    } else {
        out[OFF_MINIMAP + 0] = 0.0f;
        out[OFF_MINIMAP + 1] = 0.0f;
        out[OFF_MINIMAP + 2] = 1.0f;  // no-map sentinel
    }

    // Peer comm. Mirrors Bots/bot.py:_peer_features — for each of the
    // K_PEERS nearest other bots, write (rel_dx, rel_dy, hp, peer_vx,
    // peer_vy, peer_attacking). g_bot_last_action gives us each peer's
    // most recent decoded movement direction, the equivalent of the
    // Python agent's `peer_messages` blackboard. Without this slot the
    // trained QNet sees zeros, which it never saw during training
    // (peers were almost always within range in the swarm) and the
    // policy drifts toward picking non-stay movement actions.
    struct PeerSlot { float d2, dx, dy, hp; int action; };
    PeerSlot peers[K_PEERS];
    for (int i = 0; i < K_PEERS; ++i) peers[i] = {1e30f, 0, 0, 0, 0};
    for (auto const &kv : WS_MAP) {
        int peer_ws = kv.first;
        if (peer_ws == ws_id) continue;
        WebSocket *peer_socket = kv.second;
        if (peer_socket == nullptr) continue;
        Client *peer_client = peer_socket->getUserData();
        if (peer_client == nullptr || !peer_client->verified) continue;
        if (!sim->ent_exists(peer_client->camera)) continue;
        Entity &peer_cam = sim->get_ent(peer_client->camera);
        if (!sim->ent_alive(peer_cam.player)) continue;
        Entity &peer = sim->get_ent(peer_cam.player);
        float pdx = (float)peer.x - mx;
        float pdy = (float)peer.y - my;
        float pd2 = pdx * pdx + pdy * pdy;
        auto it = g_bot_last_action.find(peer_ws);
        int peer_action = it != g_bot_last_action.end() ? it->second : 0;
        for (int k = 0; k < K_PEERS; ++k) {
            if (pd2 < peers[k].d2) {
                for (int j = K_PEERS - 1; j > k; --j) peers[j] = peers[j-1];
                peers[k] = {pd2, pdx, pdy, (float)peer.health_ratio, peer_action};
                break;
            }
        }
    }
    for (int k = 0; k < K_PEERS; ++k) {
        int base = OFF_PEER_COMM + k * COMM_PER_PEER;
        if (peers[k].d2 >= 1e29f) continue;  // empty slot — zeros
        int act = peers[k].action;
        // Inventory actions (>= NUM_MOVEMENT_ACTIONS) don't move the
        // peer — agent.py clamps them to "stay" before deriving
        // peer_vx/peer_vy. Mirror that here so the comm vector matches.
        if (act >= NUM_MOVEMENT_ACTIONS) act = 0;
        int dir = act % NUM_DIRECTIONS;
        bool peer_defend = act >= NUM_DIRECTIONS;
        out[base + 0] = peers[k].dx / OBS_SCALE;
        out[base + 1] = peers[k].dy / OBS_SCALE;
        out[base + 2] = peers[k].hp;
        out[base + 3] = DIR_TABLE[dir][0];
        out[base + 4] = DIR_TABLE[dir][1];
        out[base + 5] = peer_defend ? 0.0f : 1.0f;  // attack flag = !defend
    }
}

// Inject a synthetic Serverbound packet into the server as if it came from
// the bot's WebSocket. Mirrors what bot.py sends over the wire — same byte
// format consumed by Server/Client.cc.
static void send_to_server(int ws_id, uint8_t const *buf, size_t len) {
    auto it = WS_MAP.find(ws_id);
    if (it == WS_MAP.end() || it->second == nullptr) return;
    WebSocket *ws = it->second;
    std::string_view sv(reinterpret_cast<char const *>(buf), len);
    Client::on_message(ws, sv, 0);
}

static void ensure_spawned(int ws_id, Client *client) {
    if (client->alive()) return;
    // Wave-system respawn gate. Bots that died mid-round wait out the
    // rest of the round; only kRoundEnd (signalled by wave_tick wrapping
    // back to 0) re-arms their respawn flag. Without this every bot
    // respawns next tick and the last-man-standing trigger can never
    // resolve.
    auto it = g_bot_can_respawn.find(ws_id);
    if (it == g_bot_can_respawn.end()) {
        // First sighting — allow the initial life so the bot enters the
        // game at all.
        g_bot_can_respawn[ws_id] = true;
        it = g_bot_can_respawn.find(ws_id);
    }
    if (!it->second) return;  // dead, waiting for round end

    // kClientSpawn { string name, string map_path }. Bundled bots ride
    // along with the human's chosen map — read once from ?spawn=... and
    // forwarded verbatim. Empty string = the URL didn't override, which
    // the server treats as "keep the camera's current map" (default for a
    // fresh camera). The buffer is sized to fit the protocol cap on
    // map_path (Server/Client.cc::MAX_SPAWN_MAP_PATH_LENGTH = 128).
    std::string const &map_path = bot_spawn_map_path();
    uint8_t buf[256] = {0};
    Writer w(buf);
    w.write<uint8_t>(Serverbound::kClientSpawn);
    char name_buf[16];
    std::snprintf(name_buf, sizeof(name_buf), "bot%d", ws_id);
    std::string name(name_buf);
    w.write<std::string>(name);
    w.write<std::string>(map_path);
    send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
    // Consumed the spawn. The bot can't double-spawn within a round —
    // the next ensure_spawned call only succeeds once the round-end
    // edge re-arms the flag.
    it->second = false;
    std::printf("[botdriver] spawned ws_id=%d as '%s'\n", ws_id, name.c_str());
}

// Diagnostics: how many bot ws_ids currently have alive flowers. The harness
// polls this to verify the spawn flow without standing up a human player.
int bot_alive_count_impl() {
    int n = 0;
    for (auto const &kv : WS_MAP) {
        if (kv.first == 0) continue;  // skip the local-player ws_id
        Client *c = kv.second ? kv.second->getUserData() : nullptr;
        if (c && c->alive()) ++n;
    }
    return n;
}

// Apply a flat action index from the QNet's argmax. Action layout (matches
// agent.py):
//   0..8   : movement (stay/N/NE/E/SE/S/SW/W/NW), attack flag held
//   9..17  : same movements, defend flag held
//   18..25 : swap loadout_ids[i] ↔ loadout_ids[i+8]  (i = 0..7)
//   26..41 : delete loadout_ids[i]                    (i = 0..15)
void bot_apply_action_impl(int ws_id, int action) {
    Client *client = client_for_ws(ws_id);
    if (client == nullptr) return;
    // Tick the round-end edge detector before any spawn logic. Only the
    // first bot per tick actually flips state; the rest no-op against
    // the unchanged wave_tick.
    update_round_gate();
    // Handshake must be done before anything else. The harness calls
    // _server_on_connect(ws_id), which creates the WebSocket+Client, but the
    // Client only flips to `verified=true` after receiving a kVerify with
    // the right VERSION_HASH. Auto-send it on the first action.
    if (!client->verified) {
        uint8_t buf[16] = {0};
        Writer w(buf);
        w.write<uint8_t>(Serverbound::kVerify);
        w.write<uint64_t>(VERSION_HASH);
        send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
    }
    ensure_spawned(ws_id, client);

    // If still dead after the spawn attempt, the round-end gate is
    // closed and we're waiting out the wave. Don't dispatch any
    // action: the server rejects every kClientInput/kPetalSwap/
    // kPetalDelete from a dead client anyway (see
    // Server/Client.cc::on_message), and the trained policy run on
    // an all-zero observation tends to issue movement actions
    // every tick — pure server-side churn that the user sees as
    // dead bots "running" between rounds.
    if (!client->alive()) return;

    // Record this bot's action so other bots can splice it into their
    // peer_comm observation slot on the next tick. Mirrors bot.py's
    // `agent.publish(...)` call; see g_bot_last_action.
    g_bot_last_action[ws_id] = action;

    if (action < NUM_MOVEMENT_ACTIONS) {
        int dir = action % NUM_DIRECTIONS;
        bool use_defend = action >= NUM_DIRECTIONS;
        float vx = DIR_TABLE[dir][0] * MOVE_MAG;
        float vy = DIR_TABLE[dir][1] * MOVE_MAG;
        uint8_t flags = use_defend ? INPUT_DEFEND : INPUT_ATTACK;

        uint8_t buf[16] = {0};
        Writer w(buf);
        w.write<uint8_t>(Serverbound::kClientInput);
        w.write<float>(vx);
        w.write<float>(vy);
        w.write<uint8_t>(flags);
        send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
        return;
    }
    int rel = action - NUM_MOVEMENT_ACTIONS;
    if (rel < 8) {
        // Swap action.
        uint8_t buf[16] = {0};
        Writer w(buf);
        w.write<uint8_t>(Serverbound::kPetalSwap);
        w.write<uint8_t>((uint8_t)rel);
        w.write<uint8_t>((uint8_t)(rel + 8));
        send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
        return;
    }
    rel -= 8;
    if (rel < 16) {
        uint8_t buf[16] = {0};
        Writer w(buf);
        w.write<uint8_t>(Serverbound::kPetalDelete);
        w.write<uint8_t>((uint8_t)rel);
        send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
        return;
    }
}

int bot_obs_dim_impl() { return BOT_OBS_DIM; }
int bot_num_actions_impl() { return BOT_NUM_ACTIONS; }
