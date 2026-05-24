#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Assets/Assets.hh>

#include <Client/Game.hh>
#include <Client/Input.hh>
#include <Client/Particle.hh>
#include <Client/StaticData.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

void render_petal(Renderer &ctx, Entity const &ent) {
    // Hold ALT (keyCode 18) to reveal the rarity of petals belonging to
    // other flowers: a thin ring in the rarity color is drawn just outside
    // the petal's collision radius. Skipped for our own petals since the
    // loadout UI already shows their rarities.
    if (Input::keys_pressed.contains((char)18) && !(ent.parent == Game::player_id)) {
        RenderContext c(&ctx);
        ctx.set_stroke(RARITY_COLORS[ent.petal_id.rarity]);
        ctx.set_line_width(2);
        ctx.begin_path();
        ctx.arc(0, 0, ent.radius + 3);
        ctx.stroke();
    }
    ctx.scale(ent.radius / PETAL_DATA[ent.petal_id.type][ent.petal_id.rarity].radius);
    draw_static_petal_single(ent.petal_id, ctx);
    if (ent.petal_id.rarity == RarityID::kUnique && frand() < 0.25)
        Particle::add_unique_particle(ent.x, ent.y);
}