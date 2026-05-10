#include <Client/Assets/Assets.hh>

#include <Client/StaticData.hh>

#include <Shared/Helpers.hh>
#include <Shared/StaticData.hh>

#include <cmath>

#define SET_BASE_COLOR(set_color) { if (!BIT_AT(flags, 0)) base_color = set_color; else { base_color = FLOWER_COLORS[attr.color]; } }

void draw_static_mob(MobID::T mob_id, Renderer &ctx, MobRenderAttributes attr) {
    // Wave-system rarity scaling. Server multiplies entity.radius by a
    // per-tier MOB_RADIUS_MULT (see Server/Spawn.cc). The case bodies
    // below use a mix of `radius` (the head / body circle) and hardcoded
    // offsets for decorations like wings, antennae, segments, etc. If we
    // just use attr.radius directly the decorations stay at their
    // authored sizes — only the head visibly grows. Soldier ants showed
    // this most obviously: head doubles, wings + rear ball don't.
    //
    // Fix: apply a uniform ctx.scale so *all* drawing in this function
    // grows uniformly with the entity's actual radius. The case bodies
    // continue to use `radius`, but we shadow it with the authored
    // canonical (MOB_DATA[mob_id].radius.min) so the post-scale visual
    // size still matches attr.radius. For mobs where attr.radius is
    // already the authored size (Common-tier, Common wave) the scale is
    // 1.0 and nothing changes.
    float authored_radius = MOB_DATA[mob_id].radius.lower;
    float visual_scale = (authored_radius > 0.0f) ? attr.radius / authored_radius : 1.0f;
    if (visual_scale != 1.0f) ctx.scale(visual_scale);
    float radius = (authored_radius > 0.0f) ? authored_radius : attr.radius;
    uint32_t flags = attr.flags;
    float animation_value = sinf(attr.animation);
    uint32_t seed = attr.seed;
    uint32_t base_color = 0xffffe763;
    switch(mob_id) {
        case MobID::kBabyAnt:
            SET_BASE_COLOR(0xff555555)
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(0, -7);
            ctx.qcurve_to(11, -10 + animation_value, 22, -5 + animation_value);
            ctx.move_to(0, 7);
            ctx.qcurve_to(11, 10 - animation_value, 22, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(0,0,radius);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kWorkerAnt:
            SET_BASE_COLOR(0xff555555)
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.arc(-12, 0, 10);
            ctx.fill();
            ctx.stroke();
            ctx.set_stroke(0xff292929);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(4, -7);
            ctx.qcurve_to(15, -10 + animation_value, 26, -5 + animation_value);
            ctx.move_to(4, 7);
            ctx.qcurve_to(15, 10 - animation_value, 26, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(4,0,radius);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kSoldierAnt:
            SET_BASE_COLOR(0xff555555)
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.arc(-12, 0, 10);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0x80eeeeee);
            {
                RenderContext context(&ctx);
                ctx.begin_path();
                ctx.rotate(0.1 * animation_value);
                ctx.translate(-11, -8);
                ctx.rotate(0.1 * M_PI);
                ctx.ellipse(0,0,15,7);
                ctx.fill();
            }
            {
                RenderContext context(&ctx);
                ctx.begin_path();
                ctx.rotate(-0.1 * animation_value);
                ctx.translate(-11, 8);
                ctx.rotate(-0.1 * M_PI);
                ctx.ellipse(0,0,15,7);
                ctx.fill();
            }
            ctx.set_stroke(0xff292929);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(4, -7);
            ctx.qcurve_to(15, -10 + animation_value, 26, -5 + animation_value);
            ctx.move_to(4, 7);
            ctx.qcurve_to(15, 10 - animation_value, 26, 5 - animation_value);
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.arc(4,0,radius);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kBee:
            SET_BASE_COLOR(0xffffe763)
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-25,9);
            ctx.line_to(-37,0);
            ctx.line_to(-25,-9);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.clip();
                ctx.set_fill(0xff333333);
                ctx.fill_rect(-30,-20,10,40);
                ctx.fill_rect(-10,-20,10,40);
                ctx.fill_rect(10,-20,10,40);
            }
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25,-5);
            ctx.qcurve_to(35,-5,40,-15);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(40,-15,5);
            ctx.fill();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25,5);
            ctx.qcurve_to(35,5,40,15);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(40,15,5);
            ctx.fill();
            break;
        case MobID::kLadybug:
        case MobID::kMassiveLadybug:
        case MobID::kDarkLadybug:
        case MobID::kShinyLadybug:
            ctx.scale(radius / 30);
            if (mob_id == MobID::kDarkLadybug) SET_BASE_COLOR(0xff962921)
            else if (mob_id == MobID::kShinyLadybug) SET_BASE_COLOR(0xffebeb34)
            else SET_BASE_COLOR(0xffeb4034)
            ctx.set_fill(0xff111111);
            ctx.begin_path();
            ctx.arc(15,0,18.5);
            ctx.fill();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.move_to(24.760068893432617,16.939273834228516);
            ctx.qcurve_to(17.74359130859375,27.195226669311523,5.530136585235596,29.485883712768555);
            ctx.qcurve_to(-6.683317184448242,31.77654457092285,-16.939273834228516,24.760068893432617);
            ctx.qcurve_to(-27.195226669311523,17.74359130859375,-29.485883712768555,5.530136585235596);
            ctx.qcurve_to(-31.77654457092285,-6.683317184448242,-24.760068893432617,-16.939273834228516);
            ctx.qcurve_to(-17.74359130859375,-27.195226669311523,-5.530136585235596,-29.485883712768555);
            ctx.qcurve_to(6.683317184448242,-31.77654457092285,16.939273834228516,-24.760068893432617);
            ctx.qcurve_to(19.241104125976562,-23.185302734375,21.213199615478516,-21.213205337524414);
            ctx.qcurve_to(23.1852970123291,-19.241111755371094,24.76006507873535,-16.939281463623047);
            ctx.qcurve_to(10,0,24.760068893432617,16.939273834228516);
            ctx.fill(1);
            {
                RenderContext context(&ctx);
                ctx.clip();
                if (mob_id == MobID::kDarkLadybug) ctx.set_fill(Renderer::HSV(base_color, 1.2));
                else ctx.set_fill(0xff111111);
                SeedGenerator gen(seed * 374572 + 46237);
                uint32_t ct = 1 + gen.next() * 7;
                for (uint32_t i = 0; i < ct; ++i) {
                    ctx.begin_path();
                    ctx.arc(gen.binext()*30,gen.binext()*30,4+gen.next()*5);
                    ctx.fill();
                }
            }
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(27.64874267578125,18.915523529052734);
            ctx.qcurve_to(19.813682556152344,30.36800765991211,6.175320625305176,32.925907135009766);
            ctx.qcurve_to(-7.463029861450195,35.48381042480469,-18.91551971435547,27.648746490478516);
            ctx.qcurve_to(-30.36800765991211,19.813682556152344,-32.925907135009766,6.175320625305176);
            ctx.qcurve_to(-35.48381042480469,-7.463029861450195,-27.648746490478516,-18.91551971435547);
            ctx.qcurve_to(-19.813682556152344,-30.36800765991211,-6.175320625305176,-32.925907135009766);
            ctx.qcurve_to(7.463029861450195,-35.48381042480469,18.91551971435547,-27.648746490478516);
            ctx.qcurve_to(24.10110092163086,-24.101102828979492,27.648740768432617,-18.915529251098633);
            ctx.qcurve_to(28.323867797851562,-17.928699493408203,28.25410270690918,-16.73506736755371);
            ctx.qcurve_to(28.184337615966797,-15.541434288024902,27.398849487304688,-14.639973640441895);
            ctx.qcurve_to(14.642288208007812,0,27.398853302001953,14.639965057373047);
            ctx.qcurve_to(28.184343338012695,15.541427612304688,28.254106521606445,16.735061645507812);
            ctx.qcurve_to(28.323869705200195,17.928693771362305,27.64874267578125,18.9155216217041);
            ctx.line_to(27.64874267578125,18.915523529052734);
            ctx.move_to(21.871395111083984,14.963025093078613);
            ctx.line_to(24.760068893432617,16.939273834228516);
            ctx.line_to(22.12128448486328,19.238582611083984);
            ctx.qcurve_to(5.3577117919921875,0,22.121280670166016,-19.238590240478516);
            ctx.line_to(24.76006507873535,-16.939281463623047);
            ctx.line_to(21.871389389038086,-14.963033676147461);
            ctx.qcurve_to(19.065046310424805,-19.0650577545166,14.96302318572998,-21.871395111083984);
            ctx.qcurve_to(5.903592586517334,-28.06928253173828,-4.884955406188965,-26.045866012573242);
            ctx.qcurve_to(-15.673511505126953,-24.022449493408203,-21.871395111083984,-14.96302318572998);
            ctx.qcurve_to(-28.06928253173828,-5.903592586517334,-26.045866012573242,4.884955406188965);
            ctx.qcurve_to(-24.022449493408203,15.673511505126953,-14.96302318572998,21.871395111083984);
            ctx.qcurve_to(-5.903592586517334,28.06928253173828,4.884955406188965,26.045866012573242);
            ctx.qcurve_to(15.673511505126953,24.022449493408203,21.871395111083984,14.963025093078613);
            ctx.fill(1);
            break;
        case MobID::kBeetle:
        case MobID::kMassiveBeetle:
            ctx.scale(radius / 35);
            SET_BASE_COLOR(0xff905db0)
            ctx.begin_path();
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.translate(35,0);
            {
                RenderContext context(&ctx);
                ctx.rotate(-0.1 * animation_value);
                ctx.move_to(-10,15);
                ctx.qcurve_to(15,30,35,15);
                ctx.qcurve_to(15,20,-10,5);
                ctx.line_to(-10,15);
                ctx.fill();
                ctx.stroke();
            }
            {
                RenderContext context(&ctx);
                ctx.rotate(0.1 * animation_value);
                ctx.move_to(-10,-15);
                ctx.qcurve_to(15,-30,35,-15);
                ctx.qcurve_to(15,-20,-10,-5);
                ctx.line_to(-10,-15);
                ctx.fill();
                ctx.stroke();
            }
            ctx.translate(-35,0);
            ctx.begin_path();
            ctx.move_to(0,-30);
            ctx.qcurve_to(40,-30,40,0);
            ctx.qcurve_to(40,30,0,30);
            ctx.qcurve_to(-40,30,-40,0);
            ctx.qcurve_to(-40,-30,0,-30);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(0,-33.5);
            ctx.qcurve_to(43.5,-33.5,43.5,0);
            ctx.qcurve_to(43.5,33.5,0,33.5);
            ctx.qcurve_to(-43.5,33.5,-43.5,0);
            ctx.qcurve_to(-43.5,-33.5,0,-33.5);
            ctx.move_to(0,-26.5);
            ctx.qcurve_to(-36.5,-26.5,-36.5,0);
            ctx.qcurve_to(-36.5,26.5,0,26.5);
            ctx.qcurve_to(36.5,26.5,36.5,0);
            ctx.qcurve_to(36.5,-26.5,0,-26.5);
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(-20,0);
            ctx.qcurve_to(0,-3,20,0);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(-17,-12);
            ctx.arc(-17,-12,5);
            ctx.move_to(-17,-2);
            ctx.arc(-17,12,5);
            ctx.move_to(0,-15);
            ctx.arc(0,-15,5);
            ctx.move_to(0,15);
            ctx.arc(0,15,5);
            ctx.move_to(17,-12);
            ctx.arc(17,-12,5);
            ctx.move_to(17,12);
            ctx.arc(17,12,5);
            ctx.fill();
            break;
        case MobID::kHornet:
            SET_BASE_COLOR(0xffffe763)
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-25,-6);
            ctx.line_to(-47,0);
            ctx.line_to(-25,6);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(base_color);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.clip();
                ctx.set_fill(0xff333333);
                ctx.fill_rect(-30,-20,10,40);
                ctx.fill_rect(-10,-20,10,40);
                ctx.fill_rect(10,-20,10,40);
            }
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.ellipse(0,0,30,20);
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(25, 5);
            ctx.qcurve_to(40, 10, 50, 15);
            ctx.qcurve_to(40, 5, 25, 5);
            ctx.move_to(25, -5);
            ctx.qcurve_to(40, -10, 50, -15);
            ctx.qcurve_to(40, -5, 25, -5);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kCactus: {
            SET_BASE_COLOR(0xff32a852)
            uint32_t vertices = radius / 10 + 5;
            {
                RenderContext context(&ctx);
                ctx.set_fill(0xff222222);
                ctx.begin_path();
                for (uint32_t i = 0; i < vertices; ++i) {
                    ctx.move_to(10+radius,0);
                    ctx.line_to(0.5+radius,3);
                    ctx.line_to(0.5+radius,-3);
                    ctx.line_to(10+radius,0);
                    ctx.rotate(M_PI * 2 / vertices);
                }
                ctx.fill();
            }
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(radius,0);
            for (uint32_t i = 0; i < vertices; ++i) {
                float base_angle = M_PI * 2 * i / vertices;
                ctx.qcurve_to(radius*0.9*cosf(base_angle+M_PI/vertices),radius*0.9*sinf(base_angle+M_PI/vertices),radius*cosf(base_angle+2*M_PI/vertices),radius*sinf(base_angle+2*M_PI/vertices));
            }
            ctx.fill();
            ctx.stroke();
            break;
        }
        case MobID::kRock:
        case MobID::kBoulder: {
            SET_BASE_COLOR(0xff777777)
            SeedGenerator gen(std::floor(radius) * 1957264 + 295726);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            float deflection = radius * 0.1;
            ctx.move_to(radius + gen.binext() * deflection,gen.binext() * deflection);
            uint32_t sides = 4 + radius / 10;
            for (uint32_t i = 1; i < sides; ++i) {
                float angle = 2 * M_PI * i / sides;
                ctx.line_to(cosf(angle) * radius + gen.binext() * deflection, sinf(angle) * radius + gen.binext() * deflection);
            }
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            break;
        }
        case MobID::kCentipede:
        case MobID::kEvilCentipede:
        case MobID::kDesertCentipede:
            if (mob_id == MobID::kCentipede) SET_BASE_COLOR(0xff8ac255)
            else if (mob_id == MobID::kEvilCentipede) SET_BASE_COLOR(0xff905db0)
            else SET_BASE_COLOR(0xffd4c66e)
            ctx.set_fill(0xff333333);
            ctx.begin_path();
            ctx.arc(0,-30,15);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,30,15);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,35);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(7);
            ctx.stroke();
            if (!BIT_AT(flags, 1)) {
                ctx.begin_path();
                ctx.move_to(25,-10);
                ctx.qcurve_to(45,-10,55,-30);
                ctx.set_stroke(0xff333333);
                ctx.set_line_width(3);
                ctx.stroke();
                ctx.begin_path();
                ctx.arc(55,-30,5);
                ctx.set_fill(0xff333333);
                ctx.fill();
                ctx.begin_path();
                ctx.move_to(25,10);
                ctx.qcurve_to(45,10,55,30);
                ctx.stroke();
                ctx.begin_path();
                ctx.arc(55,30,5);
                ctx.fill();
            }
            break;
        case MobID::kSpider:
            SET_BASE_COLOR(0xff4f412e);
            ctx.set_fill(base_color);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.begin_path();
            #define draw_leg(angle) \
            { \
                float cos = cosf(angle) * 35; \
                float sin = sinf(angle) * 35; \
                ctx.move_to(0,0); \
                ctx.qcurve_to(sin * 0.8, cos * 0.5, sin, cos); \
            }
            draw_leg(-M_PI + 0.9 + sinf(attr.animation) * 0.2)
            draw_leg(-M_PI + 0.3 + cosf(attr.animation) * 0.2)
            draw_leg(-M_PI - 0.3 + sinf(attr.animation) * 0.2)
            draw_leg(-M_PI - 0.9 - cosf(attr.animation) * 0.2)
            draw_leg(-0.9 - sinf(attr.animation) * 0.2)
            draw_leg(-0.3 + cosf(attr.animation) * 0.2)
            draw_leg(0.3 - sinf(attr.animation) * 0.2)
            draw_leg(0.9 - cosf(attr.animation) * 0.2)
            #undef draw_leg
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(0,0,radius);
            ctx.fill();
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.set_line_width(5);
            ctx.stroke();
            break;
        case MobID::kSandstorm:
            SET_BASE_COLOR(0xffd5c7a6)
            ctx.set_line_width(radius / 5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_fill(base_color);
            ctx.set_stroke(base_color);
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius, sinf(angle) * radius);
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.9));
            ctx.set_stroke(Renderer::HSV(base_color, 0.9));
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius*2/3, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius*2/3, sinf(angle) * radius*2/3);
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.rotate(attr.animation / 3);
            ctx.begin_path();
            ctx.move_to(radius/3, 0);
            for (uint32_t i = 1; i <= 6; ++i) {
                float angle = 2 * M_PI * i / 6;
                ctx.line_to(cosf(angle) * radius/3, sinf(angle) * radius/3);
            }
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kScorpion:
            ctx.scale(radius / 35);
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.round_line_join();
            {
                RenderContext context(&ctx);
                ctx.rotate(-0.05 * animation_value);
                ctx.begin_path();
                ctx.move_to(5,10.5);
                ctx.qcurve_to(30,21.5,50,10.5);
                ctx.qcurve_to(30,14,5,3.5);
                ctx.close_path();
            }
            {
                RenderContext context(&ctx);
                ctx.rotate(0.05 * animation_value);
                ctx.move_to(5,-10.5);
                ctx.qcurve_to(30,-21.5,50,-10.5);
                ctx.qcurve_to(30,-14,5,-3.5);
                ctx.close_path();
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(5);
            ctx.round_line_cap();
            ctx.begin_path();
            #define draw_leg(angle) \
            { \
                float cos = cosf(angle) * 37; \
                float sin = sinf(angle) * 37; \
                ctx.move_to(0,0); \
                ctx.qcurve_to(sin * 0.7, cos * 0.5, sin, cos); \
            }
            draw_leg(-M_PI + 0.7 + sinf(attr.animation) * 0.15)
            draw_leg(-M_PI + 0.233 + cosf(attr.animation) * 0.15)
            draw_leg(-M_PI - 0.233 + sinf(attr.animation) * 0.15)
            draw_leg(-M_PI - 0.7 - cosf(attr.animation) * 0.15)
            draw_leg(-0.7 - sinf(attr.animation) * 0.15)
            draw_leg(-0.233 + cosf(attr.animation) * 0.15)
            draw_leg(0.233 - sinf(attr.animation) * 0.15)
            draw_leg(0.7 - cosf(attr.animation) * 0.15)
            #undef draw_leg
            ctx.stroke();
            SET_BASE_COLOR(0xffc69a2d);
            ctx.set_fill(base_color);
            ctx.set_stroke(Renderer::HSV(base_color, 0.8));
            ctx.begin_path();
            ctx.move_to(0,-30);
            ctx.qcurve_to(40,-20,40,0);
            ctx.qcurve_to(40,20,0,30);
            ctx.qcurve_to(-40,35,-40,0);
            ctx.qcurve_to(-40,-35,0,-30);
            ctx.fill();
            ctx.stroke();
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.move_to(22,-12);
            ctx.qcurve_to(26,0,22,12);
            ctx.move_to(7,-18);
            ctx.qcurve_to(10.5,0,7,18);
            ctx.move_to(-7,-18);
            ctx.qcurve_to(-10.5,0,-7,18);
            ctx.move_to(-22,-15);
            ctx.qcurve_to(-27,0,-22,15);
            ctx.stroke();
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.move_to(-45, 0);
            ctx.bcurve_to(-44.9098, 9.5, -41.6136, 14.25, -32.4196, 14.2);
            ctx.bcurve_to(-23.2258, 14.15, -12.0197, 9, -8.2491, 0);
            ctx.bcurve_to(-12.0197, -9, -23.2258, -14.15, -32.4196, -14.2);
            ctx.bcurve_to(-41.6136, -14.25, -44.9098, -9.5, -45, 0);
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            ctx.begin_path();
            ctx.move_to(-37,-5);
            ctx.qcurve_to(-36,0,-37,5);
            ctx.move_to(-27,5);
            ctx.qcurve_to(-25,0,-27,-5);
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff222222);
            ctx.begin_path();
            ctx.move_to(-5.7491, 0);
            ctx.line_to(-12.7491, -7);
            ctx.line_to(-12.7491, 7);
            ctx.line_to(-5.7491, 0);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kAntHole:
            SET_BASE_COLOR(0xffb58500);
            ctx.begin_path();
            ctx.arc(0,0,radius);
            ctx.set_fill(base_color);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,radius*2/3);
            ctx.set_fill(Renderer::HSV(base_color, 0.8));
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,radius/3);
            ctx.set_fill(Renderer::HSV(base_color, 0.6));
            ctx.fill();
            break;
        case MobID::kQueenAnt:
            ctx.begin_path();
            ctx.arc(-25,0,33.5);
            ctx.set_fill(0xff454545);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(-25,0,26.5);
            ctx.set_fill(0xff555555);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,28.5);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(0,0,21.5);
            ctx.fill();
            {
                RenderContext context(&ctx);
                ctx.rotate(animation_value * 0.1);
                ctx.begin_path();
                ctx.ellipse(-14,-16,30,14,M_PI/10);
                ctx.set_fill(0x7feeeeee);
                ctx.fill();
            }
            {          
                RenderContext context(&ctx);
                ctx.rotate(-animation_value * 0.1);
                ctx.begin_path();
                ctx.ellipse(-14,16,30,14,-M_PI/10);
                ctx.set_fill(0x7feeeeee);
                ctx.fill();
            }
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(7);
            ctx.round_line_cap();
            ctx.begin_path();
            ctx.move_to(25,-10.5);
            ctx.qcurve_to(41.5,-15+2*animation_value,58,-7.5+2*animation_value);
            ctx.move_to(25,10.5);
            ctx.qcurve_to(41.5,15-2*animation_value,58,7.5-2*animation_value);
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(25,0,24.5);
            ctx.set_fill(0xff454545);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(25,0,17.5);
            ctx.set_fill(0xff555555);
            ctx.fill();
            break;
        case MobID::kSquare:
            ctx.set_fill(0xffffe869);
            ctx.set_stroke(0xffcfbc55);
            ctx.set_line_width(0.15*radius);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.rect(-radius * 0.707, -radius * 0.707, radius * 1.414, radius * 1.414);
            ctx.fill();
            ctx.stroke();
            break;
        case MobID::kDigger: {
            attr.flower_attrs.radius = attr.radius;
            attr.flower_attrs.flags |= 1;
            attr.flower_attrs.face_flags |= (1 << 7);
            draw_static_flower(ctx, attr.flower_attrs);
            break;
        };
        case MobID::kLeafbug:
            ctx.scale(radius / 20);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(2.51);
            ctx.round_line_cap();
            #define draw_leg(cx, cy, ex, ey, anim) { \
                RenderContext context(&ctx); \
                ctx.translate(-3.584, 0); \
                ctx.rotate(anim); \
                ctx.translate(3.584, 0); \
                ctx.begin_path(); \
                ctx.move_to(-3.584, 0); \
                ctx.qcurve_to(cx, cy, ex, ey); \
                ctx.stroke(); \
            }
            draw_leg(-10.416, -5.070, -12.125, -10.140,  sinf(attr.animation) * 0.1)
            draw_leg( -8.037, -6.016,  -9.151, -12.033,  cosf(attr.animation) * 0.1)
            draw_leg( -1.130, -6.450,  -0.518, -12.899, -sinf(attr.animation) * 0.1)
            draw_leg(  4.725, -4.121,   6.802,  -8.242, -cosf(attr.animation) * 0.1)
            draw_leg(-10.416,  5.070, -12.125,  10.140, -sinf(attr.animation) * 0.1)
            draw_leg( -3.937,  6.625,  -4.025,  13.251, -cosf(attr.animation) * 0.1)
            draw_leg( -1.130,  6.450,  -0.518,  12.899,  sinf(attr.animation) * 0.1)
            draw_leg(  1.502,  5.818,   2.773,  11.635,  cosf(attr.animation) * 0.1)
            #undef draw_leg

            {
                RenderContext context(&ctx);
                ctx.rotate(0.1 * animation_value);
                ctx.set_fill(0xff3c4030);
                ctx.begin_path();
                ctx.move_to(13.153, -6.960);
                ctx.bcurve_to(12.265, -7.143, 11.249, -6.662, 11.063, -5.848);
                ctx.bcurve_to(10.877, -5.033, 11.591, -4.195, 12.478, -4.010);
                ctx.bcurve_to(14.033, -3.686, 15.730, -3.288, 16.787, -2.918);
                ctx.bcurve_to(17.843, -2.549, 19.374, -1.823, 20.051, -1.359);
                ctx.bcurve_to(20.463, -1.076, 21.033, -1.121, 21.347, -1.487);
                ctx.bcurve_to(21.661, -1.852, 21.539, -2.337, 21.188, -2.683);
                ctx.bcurve_to(20.420, -3.438, 18.985, -4.473, 17.750, -5.160);
                ctx.bcurve_to(16.516, -5.847, 15.001, -6.575, 13.153, -6.959);
                ctx.fill();
            }

            {
                RenderContext context(&ctx);
                ctx.rotate(-0.1 * animation_value);
                ctx.set_fill(0xff3c4030);
                ctx.begin_path();
                ctx.move_to(13.153, 6.958);
                ctx.bcurve_to(12.265, 7.143, 11.249, 6.661, 11.063, 5.847);
                ctx.bcurve_to(10.877, 5.032, 11.591, 4.193, 12.478, 4.008);
                ctx.bcurve_to(14.033, 3.685, 15.730, 3.287, 16.787, 2.917);
                ctx.bcurve_to(17.843, 2.547, 19.374, 1.821, 20.051, 1.358);
                ctx.bcurve_to(20.463, 1.075, 21.033, 1.120, 21.347, 1.485);
                ctx.bcurve_to(21.661, 1.851, 21.539, 2.336, 21.188, 2.682);
                ctx.bcurve_to(20.420, 3.436, 18.984, 4.472, 17.750, 5.159);
                ctx.bcurve_to(16.515, 5.846, 15.001, 6.573, 13.153, 6.958);
                ctx.fill();
            }

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-8.953, 4.701);
            ctx.qcurve_to(-5.658, 6.404, -1.456, 6.711);
            ctx.qcurve_to(2.284, 6.985, 5.026, 6.112);
            ctx.qcurve_to(7.667, 5.270, 9.536, 3.464);
            ctx.qcurve_to(11.342, 1.719, 11.342, -0.001);
            ctx.qcurve_to(11.342, -1.720, 9.536, -3.466);
            ctx.qcurve_to(7.668, -5.272, 5.027, -6.113);
            ctx.qcurve_to(2.284, -6.986, -1.456, -6.712);
            ctx.qcurve_to(-5.659, -6.405, -8.953, -4.701);
            ctx.qcurve_to(-12.349, -2.945, -13.996, -1.318);
            ctx.qcurve_to(-14.977, -0.349, -14.976, -0.000);
            ctx.qcurve_to(-14.976, 0.348, -13.995, 1.317);
            ctx.qcurve_to(-12.348, 2.944, -8.953, 4.701);
            ctx.qcurve_to(-8.814, 4.773, -8.686, 4.862);
            ctx.qcurve_to(-8.557, 4.952, -8.444, 5.059);
            ctx.qcurve_to(-8.330, 5.165, -8.231, 5.286);
            ctx.qcurve_to(-8.132, 5.408, -8.051, 5.541);
            ctx.qcurve_to(-7.970, 5.675, -7.908, 5.818);
            ctx.qcurve_to(-7.846, 5.961, -7.803, 6.112);
            ctx.qcurve_to(-7.760, 6.263, -7.739, 6.417);
            ctx.qcurve_to(-7.718, 6.572, -7.718, 6.728);
            ctx.qcurve_to(-7.718, 6.728, -7.718, 6.729);
            ctx.qcurve_to(-8.860, 6.729, -10.001, 6.729);
            ctx.move_to(-11.049, 8.757);
            ctx.qcurve_to(-11.189, 8.684, -11.316, 8.595);
            ctx.qcurve_to(-11.443, 8.504, -11.558, 8.399);
            ctx.qcurve_to(-11.672, 8.292, -11.771, 8.170);
            ctx.qcurve_to(-11.870, 8.049, -11.951, 7.915);
            ctx.qcurve_to(-12.032, 7.782, -12.094, 7.639);
            ctx.qcurve_to(-12.156, 7.495, -12.199, 7.345);
            ctx.qcurve_to(-12.241, 7.194, -12.263, 7.040);
            ctx.qcurve_to(-12.284, 6.885, -12.284, 6.728);
            ctx.qcurve_to(-12.284, 6.728, -12.284, 6.728);
            ctx.qcurve_to(-11.142, 6.728, -10.001, 6.728);
            ctx.qcurve_to(-10.525, 7.742, -11.049, 8.755);
            ctx.qcurve_to(-19.543, 4.364, -19.543, -0.000);
            ctx.qcurve_to(-19.543, -4.367, -11.049, -8.758);
            ctx.qcurve_to(-6.925, -10.891, -1.788, -11.266);
            ctx.qcurve_to(2.830, -11.604, 6.413, -10.463);
            ctx.qcurve_to(10.072, -9.298, 12.710, -6.749);
            ctx.qcurve_to(15.908, -3.657, 15.908, -0.001);
            ctx.qcurve_to(15.908, 3.655, 12.710, 6.747);
            ctx.qcurve_to(10.072, 9.297, 6.412, 10.462);
            ctx.qcurve_to(2.830, 11.602, -1.788, 11.265);
            ctx.qcurve_to(-6.924, 10.890, -11.049, 8.757);
            ctx.fill();

            ctx.set_fill(0xff32a852);
            ctx.begin_path();
            ctx.move_to(-10.001, 6.729);
            ctx.bcurve_to(-4.748, 9.445, 1.850, 9.519, 5.719, 8.287);
            ctx.bcurve_to(9.588, 7.055, 13.624, 3.799, 13.625, -0.001);
            ctx.bcurve_to(13.624, -3.800, 9.588, -7.056, 5.720, -8.289);
            ctx.bcurve_to(1.850, -9.520, -4.748, -9.445, -10.001, -6.729);
            ctx.bcurve_to(-15.254, -4.013, -17.259, -1.590, -17.259, -0.000);
            ctx.bcurve_to(-17.259, 1.589, -15.253, 4.012, -10.001, 6.728);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-11.445, -0.918);
            ctx.bcurve_to(-11.952, -0.902, -12.362, -0.507, -12.363, -0.000);
            ctx.bcurve_to(-12.362, 0.506, -11.952, 0.901, -11.445, 0.916);
            ctx.qcurve_to(-1.594, 1.219, 8.258, 1.521);
            ctx.bcurve_to(9.098, 1.547, 9.779, 0.841, 9.780, -0.001);
            ctx.bcurve_to(9.780, -0.841, 9.098, -1.549, 8.258, -1.523);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(0.911, -6.164);
            ctx.bcurve_to(0.524, -6.492, -0.028, -6.521, -0.386, -6.164);
            ctx.bcurve_to(-0.744, -5.806, -0.713, -5.254, -0.386, -4.867);
            ctx.qcurve_to(2.030, -2.023, 4.446, 0.820);
            ctx.bcurve_to(4.990, 1.461, 6.004, 1.415, 6.598, 0.820);
            ctx.bcurve_to(7.193, 0.225, 7.239, -0.788, 6.598, -1.333);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(0.911, 6.163);
            ctx.bcurve_to(0.524, 6.491, -0.028, 6.521, -0.386, 6.163);
            ctx.bcurve_to(-0.743, 5.804, -0.713, 5.253, -0.386, 4.866);
            ctx.qcurve_to(2.030, 2.023, 4.445, -0.821);
            ctx.bcurve_to(4.990, -1.461, 6.003, -1.415, 6.598, -0.821);
            ctx.bcurve_to(7.193, -0.227, 7.239, 0.788, 6.598, 1.332);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-6.486, -4.831);
            ctx.bcurve_to(-6.777, -5.080, -7.194, -5.102, -7.464, -4.832);
            ctx.bcurve_to(-7.734, -4.562, -7.712, -4.145, -7.464, -3.853);
            ctx.qcurve_to(-5.641, -1.707, -3.818, 0.439);
            ctx.bcurve_to(-3.407, 0.923, -2.643, 0.888, -2.193, 0.439);
            ctx.bcurve_to(-1.744, -0.009, -1.709, -0.775, -2.193, -1.186);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-6.486, 4.831);
            ctx.bcurve_to(-6.777, 5.078, -7.194, 5.101, -7.464, 4.831);
            ctx.bcurve_to(-7.734, 4.560, -7.712, 4.144, -7.464, 3.852);
            ctx.qcurve_to(-5.641, 1.706, -3.818, -0.441);
            ctx.bcurve_to(-3.407, -0.924, -2.643, -0.889, -2.193, -0.441);
            ctx.bcurve_to(-1.744, 0.008, -1.709, 0.773, -2.193, 1.184);
            ctx.fill();
            break;
        default:
            assert(!"Didn't cover mob render");
            break;
    }
}