#ifndef WASM_SERVER
#include <Server/Server.hh>

#include <Server/Client.hh>
#include <Server/Game.hh>
#include <Shared/Config.hh>
#include <Shared/StaticDefinitions.hh>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

uWS::App Server::server = uWS::App({
    .key_file_name = "misc/key.pem",
    .cert_file_name = "misc/cert.pem",
    .passphrase = "1234"
}).ws<Client>("/*", {
    /* Settings */
    .compression = uWS::DISABLED,
    .maxPayloadLength = 128,
    .idleTimeout = 15,
    .maxBackpressure = 64 * MAX_PACKET_LEN,
    .closeOnBackpressureLimit = true,
    .resetIdleTimeoutOnSend = false,
    .sendPingsAutomatically = true,
    /* Handlers */
    .upgrade = nullptr,
    .open = [](WebSocket *ws) {
        std::cout << "client connection\n";
        Client *client = ws->getUserData();
        client->ws = ws;
    },
    .message = [](WebSocket *ws, std::string_view message, uWS::OpCode opCode) {
        Client::on_message(ws, message, opCode);
    },
    .dropped = [](WebSocket *ws, std::string_view /*message*/, uWS::OpCode /*opCode*/) {
        std::cout << "dropped packet, uh oh\n";
        Client *client = ws->getUserData();
        if (client == nullptr) {
            ws->end();
            return;
        }
        client->disconnect();
        //ws->end();
        /* A message was dropped due to set maxBackpressure and closeOnBackpressureLimit limit */
    },
    .drain = [](WebSocket */*ws*/) {
        //assert(!1);
        /* Check ws->getBufferedAmount() here */
    },
    .close = [](WebSocket *ws, int code, std::string_view message) {
        Client::on_disconnect(ws, code, message);
    }
}).listen(SERVER_PORT, [](auto *listen_socket) {
    if (listen_socket) {
        std::cout << "Listening on port " << SERVER_PORT << std::endl;
    }
});

// uSockets' kqueue backend (macOS) ignores `repeat_ms` and uses `ms` as the
// timer interval — see uSockets/src/eventing/epoll_kqueue.c, which even
// comments "Bug: repeat_ms must be the same as ms, or 0". Passing
// (ms=1, repeat_ms=1000/TPS) made the server tick at 1000 Hz regardless of
// TPS, which looked like "running as fast as it can."
//
// Fix: pass the same value for both args. The timer is millisecond-resolution,
// so for TPS > 1000 we clamp the interval to 1 ms and run multiple ticks per
// fire to keep wall-clock pace.
static uint32_t TICKS_PER_FIRE = 1;

// Debug stdin reader. Runs on a dedicated thread so the blocking
// std::getline doesn't stall the main game loop. Recognised commands:
//   <number>        Set the wave-tick counter to that value (clamped to
//                   [0, WAVE_TICKS_PER_ROUND]). Use this to fast-forward
//                   into a high-rarity phase without waiting out the
//                   round — e.g. `60000\n` jumps near round end so the
//                   next tick fires kRoundEnd, while `36000\n` puts us
//                   mid-round at roughly the Epic→Legendary boundary.
//   wave <number>   Same as above, more explicit.
//   end             Set wave_tick to WAVE_TICKS_PER_ROUND so the next
//                   tick fires end_round() naturally.
//   help            Print the command list.
// Anything else is reported but otherwise ignored.
static void _stdin_loop() {
    std::cout << "[debug] stdin commands available: <wave_tick> | wave N | end | help\n";
    std::string line;
    while (std::getline(std::cin, line)) {
        // Strip leading whitespace.
        size_t i = 0;
        while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
        if (i == line.size()) continue;
        std::string trimmed = line.substr(i);
        if (trimmed == "help") {
            std::cout << "[debug] commands: <wave_tick> | wave N | end | help\n";
            continue;
        }
        if (trimmed == "end") {
            Server::game.stdin_wave_tick_override.store((int64_t)WAVE_TICKS_PER_ROUND);
            std::cout << "[debug] queued: jump to wave_tick=" << WAVE_TICKS_PER_ROUND
                      << " (will trigger end_round)\n";
            continue;
        }
        std::istringstream iss(trimmed);
        std::string head;
        iss >> head;
        int64_t target = -1;
        if (head == "wave") iss >> target;
        else { std::istringstream iss2(trimmed); iss2 >> target; }
        if (target < 0) {
            std::cout << "[debug] unrecognised: " << trimmed << "\n";
            continue;
        }
        Server::game.stdin_wave_tick_override.store(target);
        std::cout << "[debug] queued: jump to wave_tick=" << target << "\n";
    }
}

void Server::run() {
    // Sync (lockstep) mode: tick fires only when every verified client
    // sends Serverbound::kStep. The wall-clock timer below is left
    // unstarted so the simulation can't tick on its own. Net effect: the
    // bot script and the server alternate turns at whatever rate the
    // bot can produce decisions; the TPS knob is irrelevant in this mode.
    char const *sync_env = std::getenv("GARDN_SYNC");
    bool sync_mode = (sync_env != nullptr && sync_env[0] != '\0' && sync_env[0] != '0');
    Server::game.sync_mode = sync_mode;
    if (sync_mode) {
        std::cout << "[server] GARDN_SYNC=1 — running in lockstep mode "
                     "(tick on kStep from all verified clients; TPS knob ignored)\n";
    }

    struct us_loop_t *loop = (struct us_loop_t *) uWS::Loop::get();
    struct us_timer_t *delayTimer = us_create_timer(loop, 0, 0);

    int interval_ms = (TPS >= 1000) ? 1 : (int)(1000 / TPS);
    if (interval_ms < 1) interval_ms = 1;
    TICKS_PER_FIRE = (TPS * (uint32_t)interval_ms) / 1000;
    if (TICKS_PER_FIRE < 1) TICKS_PER_FIRE = 1;

    // Spawn the stdin reader as a detached background thread — the main
    // thread owns the uSockets event loop and can't block on getline.
    std::thread(_stdin_loop).detach();

    if (!sync_mode) {
        us_timer_set(delayTimer, [](us_timer_t *x){
            for (uint32_t i = 0; i < TICKS_PER_FIRE; ++i) Server::tick();
        }, interval_ms, interval_ms);
    }
    Server::server.run();
}

void Client::send_packet(uint8_t const *packet, size_t size) {
    if (ws == nullptr) return;
    std::string_view message(reinterpret_cast<char const *>(packet), size);
    ws->send(message, uWS::OpCode::BINARY, 0);
}
#endif