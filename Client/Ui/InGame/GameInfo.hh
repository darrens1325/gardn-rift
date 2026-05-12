#pragma once

#include <Client/Ui/Element.hh>

namespace Ui {
    class LevelBar final : public Element {
    public:
        LerpFloat progress;
        uint32_t level;
        LevelBar();
        virtual void on_render(Renderer &) override;
    };

    class LeaderboardSlot final : public Element {
    public:
        uint8_t pos;
        LerpFloat ratio;
        LeaderboardSlot(uint8_t);

        virtual void on_render(Renderer &) override;
    };

    class Minimap final : public Element {
    public:
        Minimap(float);
        virtual void on_render(Renderer &) override;
    };

    class OverlevelTimer final : public Element {
    public:
        OverlevelTimer(float);
        virtual void on_render(Renderer &) override;
    };

    Element *make_leaderboard();
    Element *make_level_bar();
    Element *make_minimap();
    Element *make_overlevel_indicator();
    // Top-of-screen banner shown for a few seconds after Clientbound::kRoundEnd.
    // Reads Game::round_end_anim / round_end_was_me / round_end_winner_*; the
    // element itself just renders text, the lifetime is the global state.
    Element *make_round_end_banner();
}