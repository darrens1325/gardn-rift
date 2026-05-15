// Bundle bridge: C ABI exports + in-process router between the namespaced
// client and server sides.
//
// This TU is NOT wrapped — it compiles at global scope. The wrapped client
// and server TUs expose their entrypoints in `gardn::client::*` and
// `gardn::server::*` respectively; this file declares them as `extern` and
// re-exports them under stable C names so the harness JS (Bundle/index.html)
// can call them via Emscripten's `_client_*` / `_server_*` exports.
//
// Naming scheme on the JS side (-sEXPORTED_FUNCTIONS):
//   _client_main, _client_on_message, _client_key_event, _client_mouse_event,
//   _client_wheel_event, _client_loop
//   _server_main, _server_on_connect, _server_on_disconnect, _server_on_message,
//   _server_tick, _server_incoming_buffer_ptr, _server_incoming_buffer_cap
//   _bot_make_obs, _bot_apply_action, _bot_obs_dim, _bot_num_actions

#include <Bundle/Prologue.hh>

namespace gardn::client {
    void on_message(uint8_t type, uint32_t len);
    void mouse_event(float x, float y, uint8_t type, uint8_t button);
    void key_event(char button, uint8_t type);
    void wheel_event(float wheel);
    void loop(double d, float width, float height);
    int main();

    // Runtime arena dims (Shared/MapDimensions.hh, included by each side's
    // wrapped Shared/MapDimensions.cc). Lives in the client's namespace —
    // the bridge copies from the server's after server init below.
    extern uint32_t ARENA_WIDTH;
    extern uint32_t ARENA_HEIGHT;
}

namespace gardn::server {
    void on_connect(int ws_id);
    void on_disconnect(int ws_id, int reason);
    void on_message(int ws_id, uint32_t len);
    void tick();
    uint8_t *get_incoming_buffer_ptr();
    uint32_t get_incoming_buffer_cap();
    int main();

    extern uint32_t ARENA_WIDTH;
    extern uint32_t ARENA_HEIGHT;

    // BotDriver.cc surface — see Bundle/BotDriver.cc.
    void bot_make_obs_impl(int ws_id, float *out);
    void bot_apply_action_impl(int ws_id, int action);
    int bot_obs_dim_impl();
    int bot_num_actions_impl();
    int bot_alive_count_impl();
}

extern "C" {

// Boot. Order matters: the server must initialize (load map, allocate
// simulation) before the client connects. The harness JS calls _bundle_main
// once after both modules are instantiated and the bridge is wired up.
int bundle_main() {
    gardn::server::main();
    // Server::main → GameInstance::init → TiledMap::load, which sets
    // gardn::server::ARENA_{WIDTH,HEIGHT} from whatever .tmj it actually
    // loaded. The client side has its own copy of those globals (each side
    // is in its own namespace, so they're distinct symbols) — propagate
    // the server's values into the client's so the minimap, etc. use the
    // same arena dimensions.
    gardn::client::ARENA_WIDTH  = gardn::server::ARENA_WIDTH;
    gardn::client::ARENA_HEIGHT = gardn::server::ARENA_HEIGHT;
    // The standalone client's `main()` calls `main_loop()` which under
    // GARDN_BUNDLE is a no-op (the JS harness drives the render loop via
    // requestAnimationFrame → _client_loop). So we can run the full
    // client::main() — it just sets up Game::init() and returns.
    gardn::client::main();
    return 0;
}

// Emscripten requires a real `main` for the runtime to start up. We
// initialize lazily through bundle_main() instead, because the JS bridge
// state may not be ready when Module's onRuntimeInitialized fires. Stub
// here so the link succeeds; the harness invokes _bundle_main() explicitly.
int main() {
    return 0;
}

// Client surface --------------------------------------------------------------
void client_on_message(uint8_t type, uint32_t len) {
    gardn::client::on_message(type, len);
}
void client_mouse_event(float x, float y, uint8_t type, uint8_t button) {
    gardn::client::mouse_event(x, y, type, button);
}
void client_key_event(char button, uint8_t type) {
    gardn::client::key_event(button, type);
}
void client_wheel_event(float wheel) {
    gardn::client::wheel_event(wheel);
}
void client_loop(double d, float width, float height) {
    gardn::client::loop(d, width, height);
}

// Server surface --------------------------------------------------------------
void server_on_connect(int ws_id) {
    gardn::server::on_connect(ws_id);
}
void server_on_disconnect(int ws_id, int reason) {
    gardn::server::on_disconnect(ws_id, reason);
}
void server_on_message(int ws_id, uint32_t len) {
    gardn::server::on_message(ws_id, len);
}
void server_tick() {
    gardn::server::tick();
}
// JS reads/writes raw bytes via HEAPU8 at this pointer (then calls
// _server_on_message(ws_id, len) to dispatch).
uint8_t *server_incoming_buffer_ptr() {
    return gardn::server::get_incoming_buffer_ptr();
}
uint32_t server_incoming_buffer_cap() {
    return gardn::server::get_incoming_buffer_cap();
}

// Bot surface -----------------------------------------------------------------
// JS allocates a Float32Array of length _bot_obs_dim() and passes its byte
// pointer in; C++ fills it with the 89-dim observation for the bot at ws_id.
// JS then runs ORT-web inference, takes argmax over the action logits, and
// hands the chosen action index back via _bot_apply_action.
void bot_make_obs(int ws_id, float *out) {
    gardn::server::bot_make_obs_impl(ws_id, out);
}
void bot_apply_action(int ws_id, int action) {
    gardn::server::bot_apply_action_impl(ws_id, action);
}
int bot_obs_dim() {
    return gardn::server::bot_obs_dim_impl();
}
int bot_num_actions() {
    return gardn::server::bot_num_actions_impl();
}
int bot_alive_count() {
    return gardn::server::bot_alive_count_impl();
}

}  // extern "C"
