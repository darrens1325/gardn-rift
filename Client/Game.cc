#include <Client/Game.hh>

#include <Client/Debug.hh>
#include <Client/Particle.hh>
#include <Client/Render/TiledMapRender.hh>
#include <Client/Setup.hh>
#include <Client/Storage.hh>

#include <Shared/Config.hh>

#include <emscripten.h>

#include <cmath>
#include <cstdlib>

static double g_last_time = 0;
float const MAX_TRANSITION_CIRCLE = 2500;

static int _c = setup_canvas();
static int _i = setup_inputs();

namespace Game {
    Simulation simulation;
    Renderer renderer;
    Renderer game_ui_renderer;
    Socket socket;
    Ui::Window title_ui_window;
    Ui::Window game_ui_window;
    Ui::Window other_ui_window;
    EntityID camera_id;
    EntityID player_id;
    std::string nickname;
    std::string spawn_map_path;
    std::array<uint8_t, PetalID::kNumPetals> seen_petals;
    std::array<std::array<uint8_t, RarityID::kNumRarities>, MobID::kNumMobs> seen_mobs;

    double timestamp = 0;

    // Round-end banner state. See Game.hh; set by Network.cc on receipt of
    // Clientbound::kRoundEnd, ticked down by Game::tick() each frame.
    float round_end_anim = 0;
    uint8_t round_end_was_me = 0;
    std::string round_end_winner_name;
    uint32_t round_end_winner_score = 0;

    double score = 0;
    float overlevel_timer = 0;
    float slot_indicator_opacity = 0;
    float transition_circle = 0;

    uint32_t respawn_level = 1;

    PetalID::T cached_loadout[2 * MAX_SLOT_COUNT] = {PetalID::kNone};

    uint8_t loadout_count = 5;
    uint8_t simulation_ready = 0;
    uint8_t on_game_screen = 0;
    uint8_t show_debug = 0;
    uint8_t is_mobile = check_mobile();
}

using namespace Game;

// Pull the `?spawn=...` query parameter out of window.location.search.
// Returns "" if absent or empty. Allocated by the JS side via _malloc;
// we free it after copying into std::string. URLSearchParams.get already
// percent-decodes the value, so the server sees e.g. "Map/foo/foo.tmj"
// rather than "Map%2Ffoo%2Ffoo.tmj".
static std::string read_url_spawn_param() {
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
}

void Game::init() {
    Storage::retrieve();
    reset();
    spawn_map_path = read_url_spawn_param();
    TiledMapRender::init();
    title_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::StaticText(60, "rift test v1");
            elt->x = 0;
            elt->y = -270;
            return elt;
        }()
    );
    title_ui_window.add_child(
        Ui::make_title_input_box()
    );
    title_ui_window.add_child(
        Ui::make_title_info_box()
    );
    title_ui_window.add_child(
        Ui::make_panel_buttons()
    );
    title_ui_window.add_child(
        Ui::make_settings_panel()
    );
    title_ui_window.add_child(
        Ui::make_petal_gallery()
    );
    title_ui_window.add_child(
        Ui::make_mob_gallery()
    );
    title_ui_window.add_child(
        Ui::make_changelog()
    );
    game_ui_window.add_child(
        Ui::make_death_main_screen()
    );
    game_ui_window.add_child(
        Ui::make_level_bar()
    );
    game_ui_window.add_child(
        Ui::make_minimap()
    );
    game_ui_window.add_child(
        Ui::make_loadout_backgrounds()
    );
    for (uint8_t i = 0; i < MAX_SLOT_COUNT * 2; ++i) game_ui_window.add_child(new Ui::UiLoadoutPetal(i));
    game_ui_window.add_child(
        Ui::make_leaderboard()
    );
    game_ui_window.add_child(
        Ui::make_overlevel_indicator()
    );
    game_ui_window.add_child(
        Ui::make_round_end_banner()
    );
    game_ui_window.add_child(
        Ui::make_stat_screen()
    );
    game_ui_window.add_child(
        new Ui::HContainer({
            new Ui::StaticText(20, "the gardn project")
        }, 20, 0, { .h_justify = Ui::Style::Left, .v_justify = Ui::Style::Top })
    );
    Ui::make_petal_tooltips();
    other_ui_window.add_child(
        Ui::make_debug_stats()
    );
    other_ui_window.style.no_polling = 1;
    socket.connect(WS_URL);
}

void Game::reset() {
    simulation_ready = 0;
    on_game_screen = 0;
    score = 0;
    overlevel_timer = 0;
    slot_indicator_opacity = 0;
    transition_circle = 0;
    respawn_level = 1;
    loadout_count = 5;
    camera_id = player_id = NULL_ENTITY;
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
        cached_loadout[i] = PetalID::kNone;
    simulation.reset();
}

uint8_t Game::alive() {
    return socket.ready && simulation_ready
    && simulation.ent_exists(camera_id)
    && simulation.ent_alive(simulation.get_ent(camera_id).player);
}

uint8_t Game::in_game() {
    return simulation_ready && on_game_screen
    && simulation.ent_exists(camera_id);
}

uint8_t Game::should_render_title_ui() {
    return transition_circle < MAX_TRANSITION_CIRCLE;
}

uint8_t Game::should_render_game_ui() {
    return transition_circle > 0 && simulation_ready && simulation.ent_exists(camera_id);
}

void Game::tick(double time) {
    double tick_start = Debug::get_timestamp();
    Game::timestamp = time;
    Ui::dt = time - g_last_time;
    Ui::lerp_amount = 1 - pow(1 - 0.2, Ui::dt * 60 / 1000);
    g_last_time = time;
    // Round-end banner countdown. Ui::dt is in milliseconds — convert to
    // seconds so round_end_anim is measured in "seconds remaining."
    if (Game::round_end_anim > 0) {
        Game::round_end_anim -= (float)(Ui::dt / 1000.0);
        if (Game::round_end_anim < 0) Game::round_end_anim = 0;
    }
    simulation.tick();
    
    renderer.reset();
    game_ui_renderer.set_dimensions(renderer.width, renderer.height);
    game_ui_renderer.reset();

    Ui::window_width = renderer.width;
    Ui::window_height = renderer.height;
    Ui::focused = nullptr;
    double a = Ui::window_width / 1920;
    double b = Ui::window_height / 1080;
    Ui::scale = std::max({a, b});
    if (alive()) {
        on_game_screen = 1;
        player_id = simulation.get_ent(camera_id).player;
        Entity const &player = simulation.get_ent(player_id);
        Game::loadout_count = player.loadout_count;
        for (uint32_t i = 0; i < MAX_SLOT_COUNT + Game::loadout_count; ++i) {
            cached_loadout[i] = player.loadout_ids[i];
            Game::seen_petals[cached_loadout[i]] = 1;
        }
        score = player.score;
        overlevel_timer = player.overlevel_timer;
    } else {
        player_id = NULL_ENTITY;
        overlevel_timer = 0;
    }

    if (in_game())
        transition_circle = fclamp(transition_circle * powf(1.05, Ui::dt * 60 / 1000) + Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);
    else 
        transition_circle = fclamp(transition_circle / powf(1.05, Ui::dt * 60 / 1000) - Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);

    if (Input::is_valid()) {
        if (Game::should_render_title_ui())
            title_ui_window.poll_events();
        if (Game::should_render_game_ui())
            game_ui_window.poll_events();
        other_ui_window.poll_events();
    }
    else Ui::focused = nullptr;

    if (should_render_title_ui()) {
        render_title_screen();
        Particle::tick_title(renderer, Ui::dt);
        title_ui_window.render(renderer);
    } else
        title_ui_window.on_render_skip(renderer);

    if (should_render_game_ui()) {
        RenderContext c(&renderer);
        if (should_render_title_ui()) {
            renderer.set_stroke(0xff222222);
            renderer.set_line_width(Ui::scale * 10);
            renderer.begin_path();
            renderer.arc(renderer.width / 2, renderer.height / 2, transition_circle);
            renderer.stroke();
            renderer.clip();
        }
        render_game();
        if (!Game::alive()) {
            RenderContext c(&renderer);
            renderer.reset_transform();
            renderer.set_fill(0x20000000);
            renderer.fill_rect(0,0,renderer.width,renderer.height);
        }
        game_ui_window.render(game_ui_renderer);
        renderer.set_global_alpha(0.85);
        renderer.translate(renderer.width/2,renderer.height/2);
        renderer.draw_image(game_ui_renderer);
        //process keybind petal switches
        if (Input::keys_pressed_this_tick.contains('X'))
            Game::swap_all_petals();
        else if (Input::keys_pressed_this_tick.contains('E')) 
            Ui::forward_secondary_select();
        else if (Input::keys_pressed_this_tick.contains('Q')) 
            Ui::backward_secondary_select();
        else if (Ui::UiLoadout::selected_with_keys == MAX_SLOT_COUNT) {
            for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                if (Input::keys_pressed_this_tick.contains(SLOT_KEYBINDS[i])) {
                    Ui::forward_secondary_select();
                    break;
                }
            }
        } else if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
            Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        if (Ui::UiLoadout::selected_with_keys < MAX_SLOT_COUNT 
            && Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] != PetalID::kNone) {
            if (Input::keys_pressed_this_tick.contains('T')) {
                Ui::ui_delete_petal(Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                Ui::forward_secondary_select();
            } else {
                for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                    if (Input::keys_pressed_this_tick.contains(SLOT_KEYBINDS[i])) {
                        Ui::ui_swap_petals(i, Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                        if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
                            Ui::forward_secondary_select();
                        break;
                    }
                }
            }
        }
    } else {
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        game_ui_window.on_render_skip(game_ui_renderer);
    }
        
    if (Game::timestamp - Ui::UiLoadout::last_key_select > 5000)
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
    LERP(slot_indicator_opacity, Ui::UiLoadout::selected_with_keys != MAX_SLOT_COUNT, Ui::lerp_amount);

    other_ui_window.render(renderer);

    //no rendering past this point

    if (socket.ready && alive()) send_inputs();

    if (Input::keys_pressed_this_tick.contains((char) 186)) //';'
        show_debug = !show_debug;
    if (Input::keys_pressed_this_tick.contains('\r') && !Game::alive())
        Game::spawn_in();

    //clearing operations
    simulation.post_tick();
    Storage::set();
    Input::reset();
    Debug::frame_times.push_back(Ui::dt);
    Debug::tick_times.push_back(Debug::get_timestamp() - tick_start);
}