#include <Client/Socket.hh>
#include <Client/Game.hh>

#include <Shared/Binary.hh>
#include <Shared/Config.hh>

#include <iostream>

#include <emscripten.h>

uint8_t INCOMING_PACKET[1024 * 1024] = {0};

// Plain in-namespace implementation in the BUNDLE build; Bundle/Bridge.cc
// re-exports it as `client_on_message`. The standalone client keeps the
// classic `extern "C" on_message` symbol so existing JS callers keep working.
#ifdef GARDN_BUNDLE
void on_message(uint8_t type, uint32_t len) {
    if (type == 0) {
        Writer w(INCOMING_PACKET);
        w.write<uint8_t>(Serverbound::kVerify);
        w.write<uint64_t>(VERSION_HASH);
        Game::reset();
        Game::socket.ready = 1; //force send
        Game::socket.send(w.packet, w.at - w.packet);
        Game::socket.ready = 0;
    }
    else if (type == 2) {
        Game::on_game_screen = 0;
        Game::socket.ready = 0;
    }
    else if (type == 1) {
        Game::socket.ready = 1;
        Game::on_message(INCOMING_PACKET, len);
    }
}
#else
extern "C" {
    void on_message(uint8_t type, uint32_t len) {
        if (type == 0) {
            Writer w(INCOMING_PACKET);
            w.write<uint8_t>(Serverbound::kVerify);
            w.write<uint64_t>(VERSION_HASH);
            Game::reset();
            Game::socket.ready = 1; //force send
            Game::socket.send(w.packet, w.at - w.packet);
            Game::socket.ready = 0;
        }
        else if (type == 2) {
            Game::on_game_screen = 0;
            Game::socket.ready = 0;
        }
        else if (type == 1) {
            Game::socket.ready = 1;
            Game::on_message(INCOMING_PACKET, len);
        }
    }
}
#endif

Socket::Socket() {}

void Socket::connect(std::string const url) {
    std::cout << "Connecting to " << url << '\n';
#ifdef GARDN_BUNDLE
    // No real WebSocket in the bundle. The harness JS calls _client_on_message
    // directly when the server replies, and our `send()` below dispatches
    // straight into the server side over an in-process bridge. Signal "open"
    // immediately so the handshake (kVerify) can flow.
    EM_ASM({
        Module._gardn_client_incoming_ptr = $0;
        if (Module._gardn_bridge_ready) {
            // Trigger the kVerify handshake. The bridge expects client →
            // server message #0 to mean "open"; we fire it from JS so the
            // outbound `Game::socket.send()` below has a live server.
            setTimeout(function() { _client_on_message(0, 0); }, 0);
        } else {
            // Harness sets _gardn_bridge_ready once both modules are
            // instantiated and connects this callback.
            Module._gardn_on_client_ready = function() { _client_on_message(0, 0); };
        }
    }, INCOMING_PACKET);
#else
    EM_ASM({
        let string = UTF8ToString($1);
        function connect() {
            let socket = Module.socket = new WebSocket(string);
            socket.binaryType = "arraybuffer";
            socket.onopen = function()
            {
                console.log("Connected");
                _on_message(0);
            };
            socket.onclose = function(a)
            {
                console.log("Disconnected");
                _on_message(2, a.code);
                setTimeout(connect, 1000);
            };
            socket.onmessage = function(event)
            {
                HEAPU8.set(new Uint8Array(event.data), $0);
                _on_message(1, event.data.byteLength);
            };
        }
        setTimeout(connect, 1000);
    }, INCOMING_PACKET, url.c_str());
#endif
}

void Socket::send(uint8_t *ptr, uint32_t len) {
    if (ready == 0) return;
#ifdef GARDN_BUNDLE
    // Forward straight to the server side via the bridge. The bridge
    // function is exposed by Bundle/Bridge.cc as `bundle_client_to_server`
    // and synchronously pushes the byte buffer through to the server's
    // Client::on_message for the local-player ws_id (== 0 by convention).
    EM_ASM({
        if (Module._gardn_send_from_client) {
            Module._gardn_send_from_client($0, $1);
        }
    }, ptr, len);
#else
    EM_ASM({
        if (Module.socket?.readyState == 1) {
            Module.socket.send(HEAPU8.subarray($0, $0+$1));
        }
    }, ptr, len);
#endif
}