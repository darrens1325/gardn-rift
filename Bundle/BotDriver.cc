// Server-side bot driver: builds an 89-dim observation per bot from the
// live Simulation and applies a chosen action by routing a kClientInput
// (or kPetalSwap/kPetalDelete) packet through Client::on_message — exactly
// what a real WebSocket client would do.
//
// This file is one of the SERVER wrapped sources: the build wraps it in
// `namespace gardn::server`. Forward declarations in Bundle/Bridge.cc refer
// to `gardn::server::bot_make_obs_impl` etc.
//
// MVP scope: matches agent.py / bot.py's STATE_DIM=89 and NUM_ACTIONS=42
// layout so the ONNX-exported QNet runs unmodified. Only the BASE_STATE
// 13 dims (self HP + 3 nearest hostiles × 4) are populated; the rest are
// zero. Movement actions (0..17) are decoded into (vx, vy, input_flags)
// and sent as kClientInput. Inventory actions (18..41) are stubbed.
// Spawn-on-first-tick is handled automatically: if the bot has no live
// flower we issue a kClientSpawn with a procedural name.

#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Server/Server.hh>
#include <Shared/Binary.hh>
#include <Shared/Config.hh>
#include <Shared/Entity.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>
#include <Shared/Vector.hh>

// Mirror agent.py — kept in sync with STATE_DIM and NUM_ACTIONS there.
static constexpr int BOT_OBS_DIM = 89;
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

static Client *client_for_ws(int ws_id) {
    auto it = WS_MAP.find(ws_id);
    if (it == WS_MAP.end() || it->second == nullptr) return nullptr;
    return it->second->getUserData();
}

// Encode a 89-dim observation from the current sim state for the bot at
// ws_id. Out-of-bound / dead-bot calls just zero-fill so the model
// produces deterministic output (typically biased toward action 0 = stay).
void bot_make_obs_impl(int ws_id, float *out) {
    for (int i = 0; i < BOT_OBS_DIM; ++i) out[i] = 0.0f;
    Client *client = client_for_ws(ws_id);
    if (client == nullptr || !client->verified) return;
    Simulation *sim = &Server::game.simulation;
    if (!sim->ent_exists(client->camera)) return;
    Entity &camera = sim->get_ent(client->camera);
    if (!sim->ent_alive(camera.player)) return;
    Entity &me = sim->get_ent(camera.player);

    // BASE_STATE: self HP (1) + 3 nearest hostiles × 4 features (12).
    out[0] = (float)me.health_ratio;
    struct Hostile { float d2, dx, dy, is_player, hp; };
    Hostile nearest[3] = { {1e30f,0,0,0,0}, {1e30f,0,0,0,0}, {1e30f,0,0,0,0} };
    float mx = (float)me.x, my = (float)me.y;
    EntityID me_id = me.id;
    EntityID me_team = me.team;

    sim->for_each_entity([&](Simulation *, Entity &e) {
        if (e.id == me_id) return;
        bool is_mob = e.has_component(kMob);
        bool is_flower = e.has_component(kFlower);
        if (!is_mob && !is_flower) return;
        if (e.team == me_team) return;
        if (!e.has_component(kPhysics) || !e.has_component(kHealth)) return;
        float dx = (float)e.x - mx;
        float dy = (float)e.y - my;
        float d2 = dx*dx + dy*dy;
        for (int k = 0; k < 3; ++k) {
            if (d2 < nearest[k].d2) {
                for (int j = 2; j > k; --j) nearest[j] = nearest[j-1];
                nearest[k] = { d2, dx, dy, is_flower ? 1.0f : 0.0f, (float)e.health_ratio };
                break;
            }
        }
    });
    for (int k = 0; k < 3; ++k) {
        int base = 1 + k * 4;
        out[base + 0] = nearest[k].dx / OBS_SCALE;
        out[base + 1] = nearest[k].dy / OBS_SCALE;
        out[base + 2] = nearest[k].is_player;
        out[base + 3] = nearest[k].hp;
    }
    // Remaining 76 features (peer comm / loadout rank/type/burst / drops /
    // wall rays) are zero. The QNet was trained with them populated, so
    // the bot's policy will be a degraded version of trained behavior —
    // still legal, but no inventory mgmt, no map-aware navigation. See
    // Bundle/README.md for the porting punch list.
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
    // kClientSpawn { string name }. Use a stable name per ws_id.
    uint8_t buf[64] = {0};
    Writer w(buf);
    w.write<uint8_t>(Serverbound::kClientSpawn);
    char name_buf[16];
    std::snprintf(name_buf, sizeof(name_buf), "bot%d", ws_id);
    std::string name(name_buf);
    w.write<std::string>(name);
    send_to_server(ws_id, buf, (size_t)(w.at - w.packet));
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
