#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>

#include <cstdlib>
#include <string>
#include <vector>

#include <emscripten.h>

using namespace Ui;

// The actual WebRTC peer-to-peer machinery lives in the bundle harness JS
// (index.html) as `window.gardnMp`. This panel is just its title-screen face:
// the buttons fire the gardnMp methods, and the status line reflects
// gardnMp.status. Codes (base64 WebRTC offer/answer) move through the
// clipboard — see mpReadCode/mpWriteCode in index.html — so nothing long has
// to be rendered or edited as canvas text.
//
// In the standalone client (real WebSocket server) `window.gardnMp` is absent;
// the buttons then no-op and the status line stays empty. The feature only
// does anything in the serverless bundle.

// True when this page booted as a GUEST (?join in the URL): it connects to a
// remote host instead of running its own world. Fixed for the page session.
static bool mp_is_guest() {
    return EM_ASM_INT({
        try { return new URLSearchParams(location.search).has("join") ? 1 : 0; }
        catch (e) { return 0; }
    }) != 0;
}

// Read window.gardnMp.status into a std::string. Mirrors the malloc/free
// hand-off in Game.cc::read_url_spawn_param: JS allocates the buffer, we take
// ownership and free it after copying.
static std::string mp_status() {
    char *ptr = (char *) EM_ASM_PTR({
        try {
            var s = (window.gardnMp && window.gardnMp.status) ? window.gardnMp.status : "";
            var arr = new TextEncoder().encode(s);
            var p = Module["_malloc"](arr.length + 1);
            HEAPU8.set(arr, p);
            HEAPU8[p + arr.length] = 0;
            return p;
        } catch (e) {
            var p = Module["_malloc"](1);
            HEAPU8[p] = 0;
            return p;
        }
    });
    std::string out{ptr ? ptr : ""};
    if (ptr) std::free(ptr);
    return out;
}

Element *Ui::make_multiplayer_panel() {
    bool const guest = mp_is_guest();

    std::vector<Element *> children;
    children.push_back(new Ui::StaticText(25, guest ? "Join a friend" : "Play with friends"));

    if (guest) {
        children.push_back(new Ui::StaticText(13, "Paste your friend's invite code to join"));
        children.push_back(new Ui::StaticText(13, "their world."));
        children.push_back(new Ui::Button(260, 40,
            new Ui::StaticText(16, "Paste host's invite code"),
            [](Element *, uint8_t e){ if (e == Ui::kClick)
                EM_ASM({ if (window.gardnMp && window.gardnMp.guestJoin) window.gardnMp.guestJoin(); }); },
            nullptr,
            { .fill = 0xff1dd129, .line_width = 5, .round_radius = 3 }
        ));
        children.push_back(new Ui::Button(260, 32,
            new Ui::StaticText(14, "Host your own game instead"),
            [](Element *, uint8_t e){ if (e == Ui::kClick)
                EM_ASM({ if (window.gardnMp && window.gardnMp.goHost) window.gardnMp.goHost(); }); },
            nullptr,
            { .fill = 0xff8a8a8a, .line_width = 4, .round_radius = 3 }
        ));
    } else {
        children.push_back(new Ui::StaticText(13, "Create an invite code, send it to a"));
        children.push_back(new Ui::StaticText(13, "friend, then paste the reply they send back."));
        children.push_back(new Ui::Button(260, 40,
            new Ui::StaticText(16, "Create invite code"),
            [](Element *, uint8_t e){ if (e == Ui::kClick)
                EM_ASM({ if (window.gardnMp && window.gardnMp.hostCreate) window.gardnMp.hostCreate(); }); },
            nullptr,
            { .fill = 0xff1dd129, .line_width = 5, .round_radius = 3 }
        ));
        children.push_back(new Ui::Button(260, 40,
            new Ui::StaticText(16, "Paste friend's reply code"),
            [](Element *, uint8_t e){ if (e == Ui::kClick)
                EM_ASM({ if (window.gardnMp && window.gardnMp.hostAccept) window.gardnMp.hostAccept(); }); },
            nullptr,
            { .fill = 0xff5a9fdb, .line_width = 5, .round_radius = 3 }
        ));
        children.push_back(new Ui::Button(260, 32,
            new Ui::StaticText(14, "Join a friend's game instead"),
            [](Element *, uint8_t e){ if (e == Ui::kClick)
                EM_ASM({ if (window.gardnMp && window.gardnMp.goJoin) window.gardnMp.goJoin(); }); },
            nullptr,
            { .fill = 0xff8a8a8a, .line_width = 4, .round_radius = 3 }
        ));
    }

    children.push_back(new Ui::DynamicText(14, [](){ return mp_status(); },
        { .fill = 0xfff0d27a }));

    Element *elt = new Ui::VContainer(children, 20, 10, {
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kMultiplayer && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::multiplayer = elt;
    return elt;
}
