#ifndef WASM_SERVER
#include <Server/Server.hh>

#include <Server/Client.hh>
#include <Shared/Config.hh>

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

void Server::run() {
    struct us_loop_t *loop = (struct us_loop_t *) uWS::Loop::get();
    struct us_timer_t *delayTimer = us_create_timer(loop, 0, 0);

    int interval_ms = (TPS >= 1000) ? 1 : (int)(1000 / TPS);
    if (interval_ms < 1) interval_ms = 1;
    TICKS_PER_FIRE = (TPS * (uint32_t)interval_ms) / 1000;
    if (TICKS_PER_FIRE < 1) TICKS_PER_FIRE = 1;

    us_timer_set(delayTimer, [](us_timer_t *x){
        for (uint32_t i = 0; i < TICKS_PER_FIRE; ++i) Server::tick();
    }, interval_ms, interval_ms);
    Server::server.run();
}

void Client::send_packet(uint8_t const *packet, size_t size) {
    if (ws == nullptr) return;
    std::string_view message(reinterpret_cast<char const *>(packet), size);
    ws->send(message, uWS::OpCode::BINARY, 0);
}
#endif