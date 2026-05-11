#include <Client/Setup.hh>

#include <Client/DOM.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Storage.hh>
#include <emscripten.h>

// GARDN_BUNDLE: the .cc is compiled inside `namespace gardn::client`, so the
// "extern \"C\"" exports cannot live here (they would collide with the same C
// names emitted by the server side, and `extern \"C\"` definitions inside a
// namespace still produce a global C symbol). Bundle/Bridge.cc owns the
// real client_* C ABI exports and forwards to these in-namespace impls.
#ifdef GARDN_BUNDLE
void mouse_event(float x, float y, uint8_t type, uint8_t button) {
    Input::mouse_x = x;
    Input::mouse_y = y;
    if (type == 0) {
        ++Input::num_touches;
        Input::mouse_buttons_pressed |= 1 << button;
        Input::mouse_buttons_state |= 1 << button;
    }
    else if (type == 2) {
        --Input::num_touches;
        Input::mouse_buttons_released |= 1 << button;
        Input::mouse_buttons_state &= ~(1 << button);
    }
}

void key_event(char button, uint8_t type) {
    if (type == 0) {
        Input::keys_pressed.insert(button);
        Input::keys_pressed_this_tick.insert(button);
    }
    else if (type == 1) Input::keys_pressed.erase(button);
}

void wheel_event(float wheel) {
    Input::wheel_delta = wheel;
}

void loop(double d, float width, float height) {
    Game::renderer.width = width;
    Game::renderer.height = height;
    Game::tick(d);
}
#else
extern "C" {
    void mouse_event(float x, float y, uint8_t type, uint8_t button) {
        Input::mouse_x = x;
        Input::mouse_y = y;
        if (type == 0) {
            ++Input::num_touches;
            Input::mouse_buttons_pressed |= 1 << button;
            Input::mouse_buttons_state |= 1 << button;
        }
        else if (type == 2) {
            --Input::num_touches;
            Input::mouse_buttons_released |= 1 << button;
            Input::mouse_buttons_state &= ~(1 << button);
        }
    }

    void key_event(char button, uint8_t type) {
        if (type == 0) {
            Input::keys_pressed.insert(button);
            Input::keys_pressed_this_tick.insert(button);
        }
        else if (type == 1) Input::keys_pressed.erase(button);
    }

    void wheel_event(float wheel) {
        Input::wheel_delta = wheel;
    }

    void loop(double d, float width, float height) {
        Game::renderer.width = width;
        Game::renderer.height = height;
        Game::tick(d);
    }
}
#endif

int setup_inputs() {
#ifdef GARDN_BUNDLE
    // Bundle harness installs DOM listeners on the JS side (Bundle/index.html)
    // and dispatches to _client_key_event / _client_mouse_event / _client_wheel_event,
    // so this is a no-op here.
    return 0;
#else
    EM_ASM({
        window.addEventListener("keydown", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(e.which, 0);
        });
        window.addEventListener("keyup", (e) => {
            //e.preventDefault();
            !e.repeat && _key_event(e.which, 1);
        });
        window.addEventListener("mousedown", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 0, +!!e.button);
        });
        window.addEventListener("mousemove", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 1, +!!e.button);
        });
        window.addEventListener("mouseup", (e) => {
            //e.preventDefault();
            _mouse_event(e.clientX * devicePixelRatio, e.clientY * devicePixelRatio, 2, +!!e.button);
        });
        window.addEventListener("touchstart", (e) => {
            //e.preventDefault();
            const t = e.changedTouches[0];
            _mouse_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 0, 0);
        });
        window.addEventListener("touchmove", (e) => {
            //e.preventDefault();
            const t = e.changedTouches[0];
            _mouse_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 1, 0);
        });
        window.addEventListener("touchend", (e) => {
            //e.preventDefault();
            const t = e.changedTouches[0];
            _mouse_event(t.clientX * devicePixelRatio, t.clientY * devicePixelRatio, 2, 0);
        });
        window.addEventListener("wheel", (e) => {
            //e.preventDefault();
            _wheel_event(e.deltaY);
        });
    });
    return 0;
#endif
}

void main_loop() {
#ifdef GARDN_BUNDLE
    // Bundle harness drives the render loop from JS via _client_loop.
#else
    EM_ASM({
        function loop(time)
        {
            Module.canvas.width = innerWidth * devicePixelRatio;
            Module.canvas.height = innerHeight * devicePixelRatio;
            _loop(time, innerWidth * devicePixelRatio, innerHeight * devicePixelRatio);
            requestAnimationFrame(loop);
        };
        requestAnimationFrame(loop);
    });
#endif
}

int setup_canvas() {
    EM_ASM({
        Module.canvas = document.getElementById("canvas");
        Module.canvas.width = innerWidth * devicePixelRatio;
        Module.canvas.height = innerHeight * devicePixelRatio;
        Module.canvas.oncontextmenu = function() { return false; };
        window.onbeforeunload = function(e) { return "Are you sure?"; };
        Module.ctxs = [];
        Module.availableCtxs = [];
        Module.TextDecoder = new TextDecoder('utf8');
    });
    return 0;
}

uint8_t check_mobile() {
    return EM_ASM_INT({
        return /iPhone|iPad|iPod|Android|BlackBerry/i.test(navigator.userAgent);
    });
}