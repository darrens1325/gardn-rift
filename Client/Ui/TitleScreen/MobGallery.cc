#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>
#include <Client/Ui/Extern.hh>

#include <Client/Assets/Assets.hh>
#include <Client/Game.hh>
#include <Client/StaticData.hh>

#include <Shared/Helpers.hh>

#include <algorithm>
#include <cstring>

using namespace Ui;

// Mirror of Server/Spawn.cc's per-rarity radius scale and Server/EntityFunctions/
// Death.cc's drop-rarity upgrade. Kept inline here so the gallery cards
// match exactly what the server actually spawns / drops.
static constexpr float GALLERY_MOB_RADIUS_MULT[RarityID::kNumRarities] = {
    1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.0f, 2.2f
};
static float _gallery_pow(float base, int n) {
    float r = 1.0f;
    if (n >= 0) for (int i = 0; i < n; ++i) r *= base;
    else       for (int i = 0; i < -n; ++i) r /= base;
    return r;
}
static PetalID::T _gallery_upgrade_drop(PetalID::T base_id, uint8_t target_rarity) {
    if (base_id == PetalID::kNone || base_id >= PetalID::kNumPetals) return base_id;
    if (PETAL_DATA[base_id].rarity >= target_rarity) return base_id;
    char const *name = PETAL_DATA[base_id].name;
    PetalID::T best = base_id;
    uint8_t best_rarity = PETAL_DATA[base_id].rarity;
    for (PetalID::T id = 1; id < PetalID::kNumPetals; ++id) {
        if (id == base_id) continue;
        if (std::strcmp(PETAL_DATA[id].name, name) != 0) continue;
        uint8_t r = PETAL_DATA[id].rarity;
        if (r > target_rarity) continue;
        if (r > best_rarity) {
            best = id;
            best_rarity = r;
        }
    }
    return best;
}

GalleryMob::GalleryMob(MobID::T id, float w) :
    Element(w,w,{ .fill=0xff5a9fdb, .stroke_hsv=1, .line_width=3, .round_radius=6, .v_justify=Style::Top }), id(id), rarity(MOB_DATA[id].rarity) {}

GalleryMob::GalleryMob(MobID::T id, uint8_t r, float w) :
    Element(w,w,{ .fill=0xff5a9fdb, .stroke_hsv=1, .line_width=3, .round_radius=6, .v_justify=Style::Top }), id(id), rarity(r) {}

void GalleryMob::on_render(Renderer &ctx) {
    Element::on_render(ctx);
    ctx.begin_path();
    ctx.round_rect(-width / 2, -height / 2, width, height, style.round_radius);
    ctx.clip();
    struct MobData const &data = MOB_DATA[id];
    if (id != MobID::kDigger)
        ctx.rotate(-3*M_PI/4);
    if (id == MobID::kBeetle || id == MobID::kMassiveBeetle)
        ctx.translate(-5,0);
    // Apply the wave-system per-rarity radius multiplier so e.g. a Mythic
    // Bee in the gallery is visibly bigger than a Common Bee. The base
    // (authored) radius is the average of upper/lower; we multiply by
    // GALLERY_MOB_RADIUS_MULT[rarity] to match Server/Spawn.cc.
    float radius = (data.radius.upper + data.radius.lower) / 2;
    radius *= GALLERY_MOB_RADIUS_MULT[rarity];
    if (radius > width * 0.5) ctx.scale(0.5 * width / radius);
    ctx.scale(0.5);
    draw_static_mob(id, ctx, { .radius = radius, .flower_attrs = { .color = ColorID::kGray } });
    if (data.attributes.segments > 1) {
        ctx.translate(-2 * radius, 0);
        draw_static_mob(id, ctx, { .radius = radius, .flags = 1<<1, .flower_attrs = { .color = ColorID::kGray } });
    }
}

static Element *make_mob_drops(MobID::T id, uint8_t rarity) {
    Element *elt = new Ui::HContainer({}, 0, 6, { .h_justify = Style::Left });
    struct MobData const &data = MOB_DATA[id];
    StaticArray<float, MAX_DROPS_PER_MOB> const &drop_chances = MOB_DROP_CHANCES[id];
    std::vector<uint8_t> order;
    for (uint32_t i = 0; i < data.drops.size(); ++i)
        order.push_back(i);

    std::sort(order.begin(), order.end(), [&](uint8_t a, uint8_t b) {
        return drop_chances[a] > drop_chances[b];
    });

    for (uint32_t i = 0; i < data.drops.size(); ++i) {
        uint32_t j = order[i];
        // Show the upgraded drop ID (matches Death.cc's runtime upgrade)
        // so a Mythic Bee's drops show as Mythic Tringer, not the
        // authored Common Stinger.
        PetalID::T upgraded = _gallery_upgrade_drop(data.drops[j], rarity);
        elt->add_child(new Ui::VContainer({
            new GalleryPetal(upgraded, 45),
            new StaticText(12, format_pct(drop_chances[j] * 100))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    return elt;
}

static Element *make_mob_card(MobID::T id, uint8_t rarity) {
    int delta = (int)rarity - (int)MOB_DATA[id].rarity;
    if (delta < 0) delta = 0;
    float hp_lower = MOB_DATA[id].health.lower * _gallery_pow(1.7f, delta);
    float hp_upper = MOB_DATA[id].health.upper * _gallery_pow(1.7f, delta);
    float dmg = MOB_DATA[id].damage * _gallery_pow(1.5f, delta);
    float rad_lower = MOB_DATA[id].radius.lower * GALLERY_MOB_RADIUS_MULT[rarity];
    float rad_upper = MOB_DATA[id].radius.upper * GALLERY_MOB_RADIUS_MULT[rarity];
    Element *elt = new Ui::VContainer({
        new Ui::Element(300,0),
        new Ui::HFlexContainer(
            new Ui::VContainer({
                new Ui::StaticText(18, MOB_DATA[id].name, { .fill = 0xffffffff, .h_justify = Style::Left }),
                new Ui::StaticText(14, RARITY_NAMES[rarity], { .fill = RARITY_COLORS[rarity], .h_justify = Style::Left }),
                new Ui::Element(0,2),
                new Ui::StaticParagraph(220, 14, MOB_DATA[id].description, { .h_justify = Style::Left })
            }, 0, 5),
            new GalleryMob(id, rarity, 60),
            10, 10
        ),
        new Ui::Element(0,10),
        DEBUG_ONLY(new Ui::StaticText(14, "Health: " + RangeValue(hp_lower, hp_upper).to_string(), { .fill = 0xffffff90, .h_justify = Style::Left }),)
        DEBUG_ONLY(new Ui::StaticText(14, "Damage: " + format_score(dmg), { .fill = 0xffffff90, .h_justify = Style::Left }),)
        DEBUG_ONLY(new Ui::StaticText(14, "Radius: " + RangeValue(rad_lower, rad_upper).to_string(), { .fill = 0xffffff90, .h_justify = Style::Left }),)
        new Ui::Element(0,10),
        make_mob_drops(id, rarity)
    }, 10, 0, { .fill = 0x33000000, .stroke_hsv = 1, .line_width = 3, .round_radius = 6, .v_justify = Style::Top, .no_animation = 1 });
    Element *chooser = new Ui::Choose(
        new Ui::VContainer({
            new Ui::Element(300,5),
            new Ui::StaticText(16, "?"),
            new Ui::Element(300,5)
        }, 10, 0, { .fill = 0x33000000, .stroke_hsv = 1, .line_width = 3, .round_radius = 6, .v_justify = Style::Top, .no_animation = 1 }),
        elt,
        [id, rarity](){ return Game::seen_mobs[id][rarity]; }
    );
    chooser->style.v_justify = Style::Top;
    chooser->style.no_animation = 1;
    return chooser;
}

static Element *make_scroll() {
    Element *elt = new Ui::VContainer({}, 0, 10, {});
    // Build a (mob_id, rarity) list for every reachable tier — i.e.
    // every rarity ≥ the mob's authored rarity, since the spawn floor
    // never lets a Massive Ladybug (Epic-authored) drop to Common.
    struct Entry { MobID::T id; uint8_t rarity; };
    std::vector<Entry> entries;
    for (MobID::T i = 0; i < MobID::kNumMobs; ++i)
        for (uint8_t r = MOB_DATA[i].rarity; r < RarityID::kNumRarities; ++r)
            entries.push_back({i, r});
    std::sort(entries.begin(), entries.end(), [](Entry const &a, Entry const &b) {
        if (a.rarity != b.rarity) return a.rarity < b.rarity;
        if (MOB_DATA[a.id].rarity != MOB_DATA[b.id].rarity)
            return MOB_DATA[a.id].rarity < MOB_DATA[b.id].rarity;
        return strcmp(MOB_DATA[a.id].name, MOB_DATA[b.id].name) <= 0;
    });

    for (Entry const &e : entries)
        elt->add_child(make_mob_card(e.id, e.rarity));

    return new Ui::ScrollContainer(elt, 300);
}

Element *Ui::make_mob_gallery() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Mob Gallery"),
        make_scroll()
    }, 15, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kMobs && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::mob_gallery = elt;
    return elt;
}