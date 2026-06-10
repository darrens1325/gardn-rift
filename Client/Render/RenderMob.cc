#include <Client/Render/RenderEntity.hh>

#include <Client/Render/Renderer.hh>

#include <Client/Assets/Assets.hh>

#include <Shared/Entity.hh>
#include <Shared/StaticData.hh>

#include <Client/Game.hh>

#include <cmath>
#include <vector>

// The Leech renders as one smooth tube through every segment, drawn by the
// head (body segments draw nothing). Ported from ~/flooooio's MobRendererLeech
// (prepareNPointSmoothCurve + strokeBodyCurve). Drawing the whole body in two
// passes — full dark outline, then the lighter fill — is what keeps the joints
// from banding.
static void render_leech_body(Renderer &ctx, Entity const &head) {
    // The client doesn't receive segment links (seg_head is server-only), so
    // reconstruct the chain order by nearest-neighbour from the head. Segments
    // are kept ~2*radius apart, so a distance cap keeps separate leeches apart.
    std::vector<Entity const*> segs;
    Game::simulation.for_each<kMob>([&](Simulation *sim, Entity const &e){
        if (e.mob_id == MobID::kLeech) segs.push_back(&e);
    });
    std::vector<char> used(segs.size(), 0);
    for (size_t i = 0; i < segs.size(); ++i)
        if (segs[i] == &head) { used[i] = 1; break; }
    float R = head.radius;
    float maxStep2 = (4.0f * R) * (4.0f * R);
    std::vector<std::pair<float, float>> lp;
    float ca = cosf(head.angle), sa = sinf(head.angle);
    Entity const *cur = &head;
    for (int guard = 0; guard < 64; ++guard) {
        // World -> head-local (the ctx is already translated to the head and
        // rotated by head.angle, so undo that to place each segment in world).
        float dx = cur->x - head.x, dy = cur->y - head.y;
        lp.push_back({dx * ca + dy * sa, -dx * sa + dy * ca});
        int best = -1; float bestd = maxStep2;
        for (size_t i = 0; i < segs.size(); ++i) {
            if (used[i]) continue;
            float ex = segs[i]->x - cur->x, ey = segs[i]->y - cur->y;
            float d = ex * ex + ey * ey;
            if (d < bestd) { bestd = d; best = (int)i; }
        }
        if (best < 0) break;
        used[best] = 1; cur = segs[best];
    }
    ctx.round_line_cap();
    ctx.round_line_join();
    if (lp.size() < 2) {
        ctx.set_fill(0xff292929);
        ctx.begin_path(); ctx.arc(lp[0].first, lp[0].second, R); ctx.fill();
        ctx.set_fill(0xff333333);
        ctx.begin_path(); ctx.arc(lp[0].first, lp[0].second, R * 0.82f); ctx.fill();
    } else {
        // Catmull-Rom-style smooth curve through the segment centers.
        auto build = [&](){
            ctx.begin_path();
            ctx.move_to(lp[0].first, lp[0].second);
            for (size_t i = 0; i + 1 < lp.size(); ++i) {
                auto p0 = (i >= 1) ? lp[i - 1] : lp[0];
                auto p1 = lp[i], p2 = lp[i + 1];
                auto p3 = (i != lp.size() - 2) ? lp[i + 2] : p2;
                ctx.bcurve_to(p1.first + (p2.first - p0.first) / 6, p1.second + (p2.second - p0.second) / 6,
                              p2.first - (p3.first - p1.first) / 6, p2.second - (p3.second - p1.second) / 6,
                              p2.first, p2.second);
            }
        };
        build();
        ctx.set_stroke(0xff292929);
        ctx.set_line_width(2 * R);
        ctx.stroke();
        build();
        ctx.set_stroke(0xff333333);
        ctx.set_line_width(2 * R * 0.82f);
        ctx.stroke();
    }
    // Beak on the head (its facing is the ctx's +x).
    RenderContext bc(&ctx);
    ctx.scale(R / 20.0f);
    ctx.round_line_cap();
    ctx.set_stroke(0xff292929);
    ctx.set_line_width(4);
    ctx.begin_path(); ctx.move_to(0, 10); ctx.qcurve_to(11, 10, 22, 5); ctx.stroke();
    ctx.begin_path(); ctx.move_to(0, -10); ctx.qcurve_to(11, -10, 22, -5); ctx.stroke();
}

void render_mob(Renderer &ctx, Entity const &ent) {
    if (ent.mob_id == MobID::kLeech) {
        // Head draws the whole tube; body segments are drawn by the head.
        if (!ent.is_tail) render_leech_body(ctx, ent);
        if (ent.deletion_animation > 0) {
            uint8_t r = ent.mob_rarity;
            if (r >= RarityID::kNumRarities) r = RarityID::kNumRarities - 1;
            Game::seen_mobs[ent.mob_id][r] = 1;
        }
        return;
    }
    uint32_t flags = 0;
    flags |= (ent.team == Game::simulation.get_ent(Game::camera_id).team);
    flags |= (ent.is_tail << 1);
    MobRenderAttributes attrs = {ent.animation, ent.radius, ent.id.id, flags, ent.color};
    if (ent.has_component(kFlower)) {
        attrs.flower_attrs = {
            .radius = ent.radius,
            .eye_x = ent.eye_x,
            .eye_y = ent.eye_y,
            .mouth = ent.mouth,
            .cutter_angle = (float) (Game::timestamp / 200),
            .face_flags = ent.face_flags,
            .flags = static_cast<uint8_t>(1 | ((ent.deletion_animation > 0 ? 1 : 0) << 1)),
            .color = ent.color
        };
    }
    draw_static_mob(ent.mob_id, ctx, attrs);
    if (ent.deletion_animation > 0) {
        uint8_t r = ent.mob_rarity;
        if (r >= RarityID::kNumRarities) r = RarityID::kNumRarities - 1;
        Game::seen_mobs[ent.mob_id][r] = 1;
    }
    /*
    #ifdef DEBUG
    ctx.set_stroke(0x80ff0000);
    ctx.set_line_width(1);
    ctx.begin_path();
    ctx.arc(0,0,MOB_DATA[ent.mob_id].attributes.aggro_radius);
    ctx.stroke();
    #endif
    */
}