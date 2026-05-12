// Round-end banner: a centered top-of-screen pop-up that fires whenever
// the server broadcasts Clientbound::kRoundEnd. The actual countdown is in
// Game::tick() (round_end_anim ticked down each frame in seconds); this
// file just renders the text and gates visibility via should_render.
//
// Two variants based on round_end_was_me:
//   1: "VICTORY!"            — the local player's name matched winner_name
//   0: "{winner_name} wins!"  — somebody else won the round
// Both lines include the winner's score, formatted with the existing
// format_score helper used elsewhere in the UI.

#include <Client/Ui/InGame/GameInfo.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/DynamicText.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>

#include <Shared/Helpers.hh>

using namespace Ui;

namespace {
    // Pull from Game::round_end_* on every frame so the banner reflects
    // whichever round just ended (the strings can change while the banner
    // is still fading if a fast back-to-back round-end happens).
    std::string _banner_title() {
        if (Game::round_end_was_me) return std::string("VICTORY!");
        if (Game::round_end_winner_name.empty()) return std::string("Round over");
        return Game::round_end_winner_name + std::string(" wins!");
    }
    std::string _banner_score() {
        return std::string("Score: ") + format_score(Game::round_end_winner_score);
    }
}

Element *Ui::make_round_end_banner() {
    Element *elt = new Ui::VContainer({
        new Ui::DynamicText(36, &_banner_title, {
            .fill = 0xfff9d54a,
            .h_justify = Style::Center
        }),
        new Ui::Element(0, 6),
        new Ui::DynamicText(18, &_banner_score, {
            .fill = 0xffffffff,
            .h_justify = Style::Center
        }),
    }, 18, 6, {
        .fill = 0x80000000,
        .stroke_hsv = 1,
        .line_width = 3,
        .round_radius = 8,
        .animate = [](Element *e, Renderer &ctx){
            // round_end_anim sits in [0, 6] seconds. Fade in for the first
            // ~0.3 s, hold full opacity, fade out over the final ~0.6 s so
            // the banner doesn't pop in or out abruptly.
            float t = Game::round_end_anim;
            float a;
            if (t > 5.7f) a = (6.0f - t) / 0.3f;
            else if (t < 0.6f) a = t / 0.6f;
            else a = 1.0f;
            if (a > 1.0f) a = 1.0f;
            if (a < 0.0f) a = 0.0f;
            ctx.set_global_alpha(a * e->animation);
        },
        .should_render = [](){
            return Game::round_end_anim > 0;
        },
        .v_justify = Style::Top,
        .h_justify = Style::Center,
    });
    elt->y = 80;
    return elt;
}
