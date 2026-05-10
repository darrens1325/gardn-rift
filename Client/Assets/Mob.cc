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
            ctx.scale(radius / 10);
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
        case MobID::kBush:
            ctx.scale(radius / 10);
            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-5.895, -11.657);
            ctx.qcurve_to(-5.882, -11.651, -5.869, -11.644);
            ctx.qcurve_to(-6.007, -11.442, -6.130, -11.054);
            ctx.qcurve_to(-6.489, -9.927, -6.501, -7.943);
            ctx.qcurve_to(-6.512, -6.160, -5.757, -4.255);
            ctx.qcurve_to(-5.730, -4.186, -5.710, -4.114);
            ctx.qcurve_to(-5.689, -4.042, -5.675, -3.970);
            ctx.qcurve_to(-5.662, -3.897, -5.656, -3.822);
            ctx.qcurve_to(-5.650, -3.748, -5.651, -3.673);
            ctx.qcurve_to(-5.652, -3.599, -5.660, -3.525);
            ctx.qcurve_to(-5.669, -3.451, -5.685, -3.379);
            ctx.qcurve_to(-5.700, -3.306, -5.723, -3.234);
            ctx.qcurve_to(-5.746, -3.164, -5.775, -3.095);
            ctx.qcurve_to(-5.805, -3.027, -5.841, -2.962);
            ctx.qcurve_to(-5.877, -2.896, -5.919, -2.835);
            ctx.qcurve_to(-5.962, -2.774, -6.010, -2.718);
            ctx.qcurve_to(-6.058, -2.661, -6.111, -2.609);
            ctx.qcurve_to(-6.165, -2.557, -6.223, -2.510);
            ctx.qcurve_to(-6.281, -2.464, -6.344, -2.424);
            ctx.qcurve_to(-6.406, -2.383, -6.473, -2.349);
            ctx.qcurve_to(-6.539, -2.315, -6.608, -2.288);
            ctx.qcurve_to(-6.697, -2.252, -6.790, -2.229);
            ctx.qcurve_to(-8.774, -1.720, -10.209, -0.663);
            ctx.qcurve_to(-11.809, 0.513, -12.510, 1.466);
            ctx.qcurve_to(-12.751, 1.794, -12.833, 2.024);
            ctx.qcurve_to(-12.846, 2.018, -12.859, 2.011);
            ctx.qcurve_to(-12.849, 2.001, -12.839, 1.991);
            ctx.qcurve_to(-12.689, 2.185, -12.358, 2.422);
            ctx.qcurve_to(-11.397, 3.111, -9.513, 3.736);
            ctx.qcurve_to(-7.820, 4.297, -5.777, 4.168);
            ctx.qcurve_to(-5.702, 4.164, -5.627, 4.167);
            ctx.qcurve_to(-5.553, 4.169, -5.479, 4.180);
            ctx.qcurve_to(-5.406, 4.189, -5.333, 4.206);
            ctx.qcurve_to(-5.261, 4.223, -5.190, 4.247);
            ctx.qcurve_to(-5.120, 4.272, -5.052, 4.302);
            ctx.qcurve_to(-4.985, 4.333, -4.920, 4.371);
            ctx.qcurve_to(-4.856, 4.408, -4.795, 4.451);
            ctx.qcurve_to(-4.735, 4.494, -4.679, 4.544);
            ctx.qcurve_to(-4.623, 4.594, -4.572, 4.648);
            ctx.qcurve_to(-4.521, 4.702, -4.476, 4.762);
            ctx.qcurve_to(-4.431, 4.821, -4.392, 4.884);
            ctx.qcurve_to(-4.353, 4.948, -4.320, 5.014);
            ctx.qcurve_to(-4.288, 5.081, -4.261, 5.151);
            ctx.qcurve_to(-4.235, 5.221, -4.216, 5.293);
            ctx.qcurve_to(-4.197, 5.365, -4.185, 5.438);
            ctx.qcurve_to(-4.173, 5.511, -4.168, 5.586);
            ctx.qcurve_to(-4.162, 5.681, -4.168, 5.777);
            ctx.qcurve_to(-4.297, 7.820, -3.736, 9.513);
            ctx.qcurve_to(-3.111, 11.396, -2.422, 12.358);
            ctx.qcurve_to(-2.185, 12.688, -1.991, 12.838);
            ctx.qcurve_to(-2.001, 12.848, -2.012, 12.859);
            ctx.qcurve_to(-2.018, 12.846, -2.024, 12.833);
            ctx.qcurve_to(-1.794, 12.750, -1.466, 12.510);
            ctx.qcurve_to(-0.513, 11.808, 0.663, 10.209);
            ctx.qcurve_to(1.720, 8.772, 2.229, 6.790);
            ctx.qcurve_to(2.247, 6.718, 2.273, 6.648);
            ctx.qcurve_to(2.298, 6.578, 2.330, 6.511);
            ctx.qcurve_to(2.362, 6.444, 2.401, 6.380);
            ctx.qcurve_to(2.440, 6.316, 2.485, 6.257);
            ctx.qcurve_to(2.529, 6.197, 2.579, 6.143);
            ctx.qcurve_to(2.629, 6.088, 2.685, 6.038);
            ctx.qcurve_to(2.741, 5.988, 2.801, 5.944);
            ctx.qcurve_to(2.860, 5.900, 2.925, 5.862);
            ctx.qcurve_to(2.989, 5.824, 3.056, 5.793);
            ctx.qcurve_to(3.124, 5.761, 3.194, 5.737);
            ctx.qcurve_to(3.264, 5.712, 3.336, 5.694);
            ctx.qcurve_to(3.409, 5.677, 3.483, 5.666);
            ctx.qcurve_to(3.556, 5.656, 3.630, 5.652);
            ctx.qcurve_to(3.705, 5.648, 3.779, 5.653);
            ctx.qcurve_to(3.854, 5.657, 3.927, 5.668);
            ctx.qcurve_to(4.001, 5.679, 4.073, 5.698);
            ctx.qcurve_to(4.165, 5.722, 4.255, 5.757);
            ctx.qcurve_to(6.160, 6.511, 7.942, 6.501);
            ctx.qcurve_to(9.927, 6.488, 11.054, 6.130);
            ctx.qcurve_to(11.442, 6.006, 11.644, 5.869);
            ctx.qcurve_to(11.651, 5.881, 11.658, 5.894);
            ctx.qcurve_to(11.644, 5.896, 11.629, 5.898);
            ctx.qcurve_to(11.622, 5.654, 11.493, 5.267);
            ctx.qcurve_to(11.121, 4.144, 9.964, 2.533);
            ctx.qcurve_to(8.925, 1.084, 7.196, -0.014);
            ctx.qcurve_to(7.133, -0.054, 7.075, -0.100);
            ctx.qcurve_to(7.016, -0.145, 6.962, -0.197);
            ctx.qcurve_to(6.908, -0.248, 6.859, -0.304);
            ctx.qcurve_to(6.811, -0.361, 6.768, -0.422);
            ctx.qcurve_to(6.726, -0.482, 6.689, -0.548);
            ctx.qcurve_to(6.652, -0.612, 6.622, -0.681);
            ctx.qcurve_to(6.592, -0.749, 6.569, -0.819);
            ctx.qcurve_to(6.545, -0.890, 6.529, -0.963);
            ctx.qcurve_to(6.513, -1.035, 6.503, -1.109);
            ctx.qcurve_to(6.495, -1.183, 6.493, -1.257);
            ctx.qcurve_to(6.491, -1.332, 6.496, -1.407);
            ctx.qcurve_to(6.502, -1.480, 6.515, -1.554);
            ctx.qcurve_to(6.528, -1.627, 6.548, -1.699);
            ctx.qcurve_to(6.568, -1.771, 6.594, -1.840);
            ctx.qcurve_to(6.622, -1.910, 6.655, -1.976);
            ctx.qcurve_to(6.688, -2.043, 6.729, -2.105);
            ctx.qcurve_to(6.779, -2.186, 6.840, -2.260);
            ctx.qcurve_to(8.145, -3.838, 8.686, -5.537);
            ctx.qcurve_to(9.288, -7.428, 9.295, -8.611);
            ctx.qcurve_to(9.297, -9.018, 9.229, -9.253);
            ctx.qcurve_to(9.243, -9.255, 9.257, -9.258);
            ctx.qcurve_to(9.255, -9.244, 9.252, -9.230);
            ctx.qcurve_to(9.018, -9.298, 8.611, -9.296);
            ctx.qcurve_to(7.427, -9.288, 5.536, -8.687);
            ctx.qcurve_to(3.838, -8.146, 2.260, -6.840);
            ctx.qcurve_to(2.203, -6.793, 2.141, -6.751);
            ctx.qcurve_to(2.079, -6.709, 2.014, -6.674);
            ctx.qcurve_to(1.948, -6.639, 1.880, -6.610);
            ctx.qcurve_to(1.811, -6.581, 1.740, -6.560);
            ctx.qcurve_to(1.668, -6.538, 1.596, -6.522);
            ctx.qcurve_to(1.523, -6.508, 1.449, -6.500);
            ctx.qcurve_to(1.375, -6.492, 1.300, -6.492);
            ctx.qcurve_to(1.225, -6.492, 1.152, -6.499);
            ctx.qcurve_to(1.078, -6.506, 1.004, -6.521);
            ctx.qcurve_to(0.931, -6.534, 0.860, -6.556);
            ctx.qcurve_to(0.789, -6.577, 0.719, -6.605);
            ctx.qcurve_to(0.651, -6.634, 0.585, -6.668);
            ctx.qcurve_to(0.519, -6.703, 0.457, -6.744);
            ctx.qcurve_to(0.395, -6.785, 0.337, -6.832);
            ctx.qcurve_to(0.279, -6.879, 0.227, -6.932);
            ctx.qcurve_to(0.174, -6.984, 0.126, -7.041);
            ctx.qcurve_to(0.065, -7.115, 0.014, -7.196);
            ctx.qcurve_to(-1.083, -8.925, -2.532, -9.965);
            ctx.qcurve_to(-4.145, -11.122, -5.268, -11.494);
            ctx.qcurve_to(-5.654, -11.622, -5.899, -11.629);
            ctx.qcurve_to(-5.897, -11.644, -5.894, -11.658);
            ctx.qcurve_to(-5.960, -11.624, -6.029, -11.597);
            ctx.qcurve_to(-6.099, -11.570, -6.171, -11.550);
            ctx.qcurve_to(-6.242, -11.530, -6.315, -11.517);
            ctx.qcurve_to(-6.389, -11.504, -6.463, -11.497);
            ctx.qcurve_to(-6.537, -11.492, -6.612, -11.493);
            ctx.qcurve_to(-6.686, -11.494, -6.760, -11.503);
            ctx.qcurve_to(-6.834, -11.512, -6.907, -11.528);
            ctx.qcurve_to(-6.980, -11.544, -7.050, -11.567);
            ctx.qcurve_to(-7.121, -11.589, -7.190, -11.619);
            ctx.qcurve_to(-7.258, -11.650, -7.323, -11.686);
            ctx.qcurve_to(-7.387, -11.722, -7.448, -11.765);
            ctx.qcurve_to(-7.510, -11.807, -7.566, -11.855);
            ctx.qcurve_to(-7.623, -11.904, -7.675, -11.957);
            ctx.qcurve_to(-7.726, -12.011, -7.772, -12.069);
            ctx.qcurve_to(-7.818, -12.128, -7.858, -12.191);
            ctx.qcurve_to(-7.898, -12.253, -7.933, -12.320);
            ctx.qcurve_to(-7.967, -12.386, -7.994, -12.455);
            ctx.qcurve_to(-8.020, -12.525, -8.041, -12.596);
            ctx.qcurve_to(-8.062, -12.668, -8.074, -12.741);
            ctx.qcurve_to(-8.087, -12.815, -8.093, -12.889);
            ctx.qcurve_to(-8.099, -12.963, -8.098, -13.038);
            ctx.qcurve_to(-8.096, -13.112, -8.087, -13.186);
            ctx.qcurve_to(-8.078, -13.260, -8.063, -13.333);
            ctx.qcurve_to(-8.047, -13.405, -8.024, -13.476);
            ctx.qcurve_to(-8.001, -13.547, -7.971, -13.615);
            ctx.qcurve_to(-7.941, -13.684, -7.905, -13.748);
            ctx.qcurve_to(-7.869, -13.813, -7.826, -13.874);
            ctx.qcurve_to(-7.783, -13.935, -7.736, -13.992);
            ctx.qcurve_to(-7.687, -14.049, -7.633, -14.101);
            ctx.qcurve_to(-7.580, -14.152, -7.521, -14.198);
            ctx.qcurve_to(-7.463, -14.244, -7.400, -14.284);
            ctx.qcurve_to(-7.338, -14.324, -7.271, -14.358);
            ctx.qcurve_to(-5.042, -15.495, -0.765, -12.427);
            ctx.qcurve_to(1.161, -11.045, 2.574, -8.820);
            ctx.qcurve_to(1.934, -8.414, 1.294, -8.008);
            ctx.qcurve_to(0.811, -8.592, 0.328, -9.175);
            ctx.qcurve_to(2.357, -10.856, 4.618, -11.575);
            ctx.qcurve_to(9.632, -13.170, 11.401, -11.401);
            ctx.qcurve_to(13.170, -9.632, 11.575, -4.618);
            ctx.qcurve_to(10.856, -2.359, 9.176, -0.328);
            ctx.qcurve_to(8.592, -0.811, 8.008, -1.293);
            ctx.qcurve_to(8.414, -1.933, 8.820, -2.573);
            ctx.qcurve_to(11.046, -1.161, 12.427, 0.765);
            ctx.qcurve_to(15.494, 5.041, 14.358, 7.271);
            ctx.qcurve_to(13.222, 9.499, 7.961, 9.531);
            ctx.qcurve_to(5.590, 9.546, 3.139, 8.575);
            ctx.qcurve_to(3.418, 7.871, 3.697, 7.166);
            ctx.qcurve_to(4.431, 7.355, 5.165, 7.543);
            ctx.qcurve_to(4.511, 10.095, 3.104, 12.006);
            ctx.qcurve_to(-0.015, 16.245, -2.485, 15.852);
            ctx.qcurve_to(-4.957, 15.461, -6.613, 10.467);
            ctx.qcurve_to(-7.360, 8.216, -7.193, 5.586);
            ctx.qcurve_to(-6.437, 5.633, -5.681, 5.681);
            ctx.qcurve_to(-5.633, 6.438, -5.586, 7.194);
            ctx.qcurve_to(-8.217, 7.359, -10.467, 6.613);
            ctx.qcurve_to(-15.462, 4.957, -15.853, 2.485);
            ctx.qcurve_to(-16.245, 0.013, -12.006, -3.105);
            ctx.qcurve_to(-10.096, -4.511, -7.543, -5.165);
            ctx.qcurve_to(-7.355, -4.431, -7.166, -3.697);
            ctx.qcurve_to(-7.871, -3.418, -8.575, -3.139);
            ctx.qcurve_to(-9.546, -5.590, -9.532, -7.961);
            ctx.qcurve_to(-9.500, -13.223, -7.270, -14.359);
            ctx.qcurve_to(-7.204, -14.393, -7.135, -14.420);
            ctx.qcurve_to(-7.065, -14.447, -6.994, -14.467);
            ctx.qcurve_to(-6.922, -14.487, -6.849, -14.500);
            ctx.qcurve_to(-6.776, -14.513, -6.702, -14.519);
            ctx.qcurve_to(-6.627, -14.525, -6.552, -14.523);
            ctx.qcurve_to(-6.478, -14.522, -6.404, -14.513);
            ctx.qcurve_to(-6.330, -14.504, -6.258, -14.489);
            ctx.qcurve_to(-6.185, -14.473, -6.114, -14.450);
            ctx.qcurve_to(-6.043, -14.426, -5.975, -14.396);
            ctx.qcurve_to(-5.907, -14.367, -5.842, -14.330);
            ctx.qcurve_to(-5.777, -14.294, -5.716, -14.251);
            ctx.qcurve_to(-5.655, -14.209, -5.598, -14.161);
            ctx.qcurve_to(-5.541, -14.112, -5.490, -14.058);
            ctx.qcurve_to(-5.438, -14.005, -5.392, -13.947);
            ctx.qcurve_to(-5.346, -13.889, -5.306, -13.825);
            ctx.qcurve_to(-5.266, -13.762, -5.232, -13.696);
            ctx.qcurve_to(-5.198, -13.630, -5.171, -13.560);
            ctx.qcurve_to(-5.145, -13.491, -5.124, -13.419);
            ctx.qcurve_to(-5.103, -13.348, -5.090, -13.274);
            ctx.qcurve_to(-5.077, -13.201, -5.072, -13.127);
            ctx.qcurve_to(-5.066, -13.053, -5.068, -12.978);
            ctx.qcurve_to(-5.069, -12.904, -5.078, -12.830);
            ctx.qcurve_to(-5.086, -12.756, -5.102, -12.683);
            ctx.qcurve_to(-5.118, -12.610, -5.141, -12.540);
            ctx.qcurve_to(-5.164, -12.469, -5.194, -12.401);
            ctx.qcurve_to(-5.224, -12.333, -5.261, -12.267);
            ctx.qcurve_to(-5.297, -12.203, -5.339, -12.141);
            ctx.qcurve_to(-5.382, -12.080, -5.430, -12.024);
            ctx.qcurve_to(-5.478, -11.967, -5.532, -11.915);
            ctx.qcurve_to(-5.587, -11.864, -5.644, -11.818);
            ctx.qcurve_to(-5.702, -11.772, -5.766, -11.732);
            ctx.qcurve_to(-5.830, -11.692, -5.895, -11.658);
            ctx.fill();

            ctx.set_fill(0xff32a852);
            ctx.begin_path();
            ctx.move_to(-6.582, -13.008);
            ctx.bcurve_to(-7.335, -12.625, -7.997, -11.092, -8.016, -7.952);
            ctx.bcurve_to(-8.026, -6.477, -7.685, -5.005, -7.166, -3.697);
            ctx.bcurve_to(-8.529, -3.348, -9.921, -2.758, -11.108, -1.884);
            ctx.bcurve_to(-13.637, -0.023, -14.488, 1.414, -14.356, 2.248);
            ctx.bcurve_to(-14.224, 3.083, -12.971, 4.186, -9.990, 5.174);
            ctx.bcurve_to(-8.591, 5.639, -7.086, 5.770, -5.681, 5.681);
            ctx.bcurve_to(-5.770, 7.085, -5.639, 8.591, -5.175, 9.990);
            ctx.bcurve_to(-4.186, 12.971, -3.082, 14.224, -2.248, 14.356);
            ctx.bcurve_to(-1.415, 14.488, 0.022, 13.637, 1.884, 11.108);
            ctx.bcurve_to(2.758, 9.920, 3.348, 8.529, 3.697, 7.166);
            ctx.bcurve_to(5.005, 7.684, 6.477, 8.025, 7.952, 8.016);
            ctx.bcurve_to(11.091, 7.997, 12.625, 7.335, 13.008, 6.582);
            ctx.bcurve_to(13.391, 5.830, 13.026, 4.200, 11.196, 1.649);
            ctx.bcurve_to(10.337, 0.451, 9.196, -0.540, 8.008, -1.293);
            ctx.bcurve_to(8.904, -2.377, 9.684, -3.673, 10.130, -5.077);
            ctx.bcurve_to(11.083, -8.070, 10.926, -9.733, 10.329, -10.329);
            ctx.bcurve_to(9.732, -10.926, 8.069, -11.083, 5.077, -10.131);
            ctx.bcurve_to(3.672, -9.684, 2.377, -8.905, 1.294, -8.008);
            ctx.bcurve_to(0.540, -9.196, -0.451, -10.336, -1.649, -11.196);
            ctx.bcurve_to(-4.201, -13.026, -5.830, -13.391, -6.582, -13.008);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-5.382, -3.443);
            ctx.bcurve_to(-5.631, -3.547, -5.778, -3.801, -5.694, -4.057);
            ctx.bcurve_to(-5.612, -4.312, -5.343, -4.431, -5.081, -4.369);
            ctx.qcurve_to(-3.154, -3.912, -1.226, -3.455);
            ctx.bcurve_to(-0.792, -3.351, -0.569, -2.861, -0.707, -2.436);
            ctx.bcurve_to(-0.845, -2.012, -1.313, -1.745, -1.725, -1.917);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(0.450, -6.415);
            ctx.bcurve_to(0.512, -6.677, 0.393, -6.945, 0.138, -7.028);
            ctx.bcurve_to(-0.118, -7.111, -0.372, -6.964, -0.476, -6.716);
            ctx.qcurve_to(-1.239, -4.887, -2.003, -3.059);
            ctx.bcurve_to(-2.174, -2.647, -1.908, -2.178, -1.483, -2.041);
            ctx.bcurve_to(-1.059, -1.903, -0.568, -2.125, -0.465, -2.560);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-6.243, -6.691);
            ctx.bcurve_to(-6.431, -6.769, -6.542, -6.960, -6.479, -7.153);
            ctx.bcurve_to(-6.417, -7.347, -6.213, -7.436, -6.016, -7.389);
            ctx.qcurve_to(-4.561, -7.044, -3.106, -6.699);
            ctx.bcurve_to(-2.778, -6.621, -2.610, -6.250, -2.714, -5.930);
            ctx.bcurve_to(-2.818, -5.610, -3.172, -5.409, -3.483, -5.538);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-1.671, -9.020);
            ctx.bcurve_to(-1.624, -9.218, -1.714, -9.420, -1.907, -9.483);
            ctx.bcurve_to(-2.101, -9.546, -2.291, -9.434, -2.370, -9.247);
            ctx.qcurve_to(-2.946, -7.867, -3.522, -6.487);
            ctx.bcurve_to(-3.652, -6.176, -3.451, -5.822, -3.130, -5.718);
            ctx.bcurve_to(-2.810, -5.614, -2.439, -5.781, -2.362, -6.109);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-4.954, -8.740);
            ctx.bcurve_to(-5.068, -8.984, -4.981, -9.273, -4.741, -9.394);
            ctx.bcurve_to(-4.502, -9.516, -4.217, -9.417, -4.086, -9.182);
            ctx.qcurve_to(-1.746, -4.943, 0.593, -0.705);
            ctx.bcurve_to(0.809, -0.314, 0.638, 0.180, 0.240, 0.382);
            ctx.bcurve_to(-0.157, 0.585, -0.657, 0.433, -0.847, 0.029);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(6.644, -7.324);
            ctx.bcurve_to(6.841, -7.508, 7.143, -7.514, 7.333, -7.324);
            ctx.bcurve_to(7.523, -7.134, 7.517, -6.832, 7.333, -6.636);
            ctx.qcurve_to(4.025, -3.100, 0.718, 0.436);
            ctx.bcurve_to(0.412, 0.762, -0.110, 0.751, -0.426, 0.436);
            ctx.bcurve_to(-0.741, 0.120, -0.752, -0.403, -0.426, -0.708);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(1.544, -6.179);
            ctx.bcurve_to(1.565, -6.447, 1.761, -6.665, 2.030, -6.665);
            ctx.bcurve_to(2.299, -6.665, 2.495, -6.447, 2.517, -6.178);
            ctx.qcurve_to(2.678, -4.203, 2.839, -2.229);
            ctx.bcurve_to(2.875, -1.784, 2.477, -1.420, 2.030, -1.420);
            ctx.bcurve_to(1.584, -1.420, 1.186, -1.784, 1.222, -2.229);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(6.172, -1.550);
            ctx.bcurve_to(6.441, -1.572, 6.659, -1.768, 6.659, -2.036);
            ctx.bcurve_to(6.659, -2.305, 6.441, -2.502, 6.172, -2.523);
            ctx.qcurve_to(4.197, -2.684, 2.222, -2.845);
            ctx.bcurve_to(1.778, -2.881, 1.414, -2.483, 1.414, -2.036);
            ctx.bcurve_to(1.414, -1.590, 1.778, -1.192, 2.223, -1.228);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(4.366, -8.000);
            ctx.bcurve_to(4.382, -8.202, 4.530, -8.368, 4.733, -8.368);
            ctx.bcurve_to(4.936, -8.368, 5.084, -8.203, 5.101, -8.001);
            ctx.qcurve_to(5.222, -6.510, 5.344, -5.019);
            ctx.bcurve_to(5.371, -4.683, 5.070, -4.409, 4.733, -4.409);
            ctx.bcurve_to(4.396, -4.409, 4.096, -4.684, 4.123, -5.019);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(7.994, -4.372);
            ctx.bcurve_to(8.197, -4.388, 8.361, -4.537, 8.361, -4.739);
            ctx.bcurve_to(8.361, -4.942, 8.197, -5.090, 7.994, -5.107);
            ctx.qcurve_to(6.504, -5.228, 5.013, -5.349);
            ctx.bcurve_to(4.677, -5.377, 4.403, -5.076, 4.403, -4.739);
            ctx.bcurve_to(4.403, -4.403, 4.677, -4.102, 5.013, -4.129);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-9.632, 0.970);
            ctx.bcurve_to(-9.897, 1.021, -10.078, 1.261, -10.037, 1.527);
            ctx.bcurve_to(-9.994, 1.792, -9.747, 1.964, -9.480, 1.932);
            ctx.qcurve_to(-4.675, 1.333, 0.129, 0.734);
            ctx.bcurve_to(0.572, 0.680, 0.870, 0.251, 0.801, -0.190);
            ctx.bcurve_to(0.731, -0.631, 0.314, -0.946, -0.124, -0.862);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-5.707, -2.484);
            ctx.bcurve_to(-5.937, -2.624, -6.229, -2.594, -6.387, -2.376);
            ctx.bcurve_to(-6.545, -2.159, -6.484, -1.872, -6.279, -1.697);
            ctx.qcurve_to(-4.776, -0.406, -3.273, 0.885);
            ctx.bcurve_to(-2.934, 1.176, -2.406, 1.067, -2.144, 0.706);
            ctx.bcurve_to(-1.882, 0.345, -1.941, -0.190, -2.322, -0.422);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-4.683, 3.981);
            ctx.bcurve_to(-4.859, 4.186, -5.145, 4.247, -5.363, 4.088);
            ctx.bcurve_to(-5.581, 3.931, -5.611, 3.639, -5.471, 3.409);
            ctx.qcurve_to(-4.440, 1.717, -3.409, 0.025);
            ctx.bcurve_to(-3.177, -0.357, -2.641, -0.417, -2.280, -0.154);
            ctx.bcurve_to(-1.919, 0.108, -1.811, 0.636, -2.101, 0.975);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-8.840, -1.272);
            ctx.bcurve_to(-9.013, -1.378, -9.233, -1.354, -9.353, -1.191);
            ctx.bcurve_to(-9.472, -1.026, -9.426, -0.810, -9.271, -0.678);
            ctx.qcurve_to(-8.137, 0.297, -7.003, 1.271);
            ctx.bcurve_to(-6.747, 1.490, -6.348, 1.409, -6.150, 1.136);
            ctx.bcurve_to(-5.952, 0.864, -5.998, 0.460, -6.285, 0.284);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-8.037, 3.796);
            ctx.bcurve_to(-8.170, 3.950, -8.386, 3.997, -8.550, 3.877);
            ctx.bcurve_to(-8.714, 3.758, -8.737, 3.538, -8.632, 3.364);
            ctx.qcurve_to(-7.854, 2.087, -7.075, 0.810);
            ctx.bcurve_to(-6.901, 0.522, -6.496, 0.477, -6.223, 0.675);
            ctx.bcurve_to(-5.951, 0.873, -5.869, 1.272, -6.088, 1.527);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(8.972, 3.981);
            ctx.bcurve_to(9.208, 4.111, 9.306, 4.396, 9.184, 4.636);
            ctx.bcurve_to(9.062, 4.875, 8.773, 4.963, 8.529, 4.848);
            ctx.qcurve_to(4.145, 2.795, -0.239, 0.741);
            ctx.bcurve_to(-0.643, 0.552, -0.795, 0.052, -0.592, -0.346);
            ctx.bcurve_to(-0.390, -0.743, 0.104, -0.915, 0.495, -0.699);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(6.306, -0.517);
            ctx.bcurve_to(6.568, -0.579, 6.836, -0.460, 6.919, -0.204);
            ctx.bcurve_to(7.002, 0.052, 6.855, 0.306, 6.607, 0.409);
            ctx.qcurve_to(4.778, 1.173, 2.950, 1.936);
            ctx.bcurve_to(2.538, 2.107, 2.069, 1.841, 1.932, 1.416);
            ctx.bcurve_to(1.794, 0.992, 2.016, 0.501, 2.451, 0.398);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(3.334, 5.316);
            ctx.bcurve_to(3.438, 5.564, 3.692, 5.711, 3.947, 5.628);
            ctx.bcurve_to(4.203, 5.545, 4.322, 5.277, 4.260, 5.015);
            ctx.qcurve_to(3.803, 3.087, 3.345, 1.159);
            ctx.bcurve_to(3.242, 0.725, 2.752, 0.502, 2.327, 0.640);
            ctx.bcurve_to(1.902, 0.778, 1.636, 1.247, 1.808, 1.659);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(8.910, 1.604);
            ctx.bcurve_to(9.108, 1.557, 9.311, 1.647, 9.374, 1.841);
            ctx.bcurve_to(9.436, 2.034, 9.325, 2.225, 9.138, 2.304);
            ctx.qcurve_to(7.758, 2.879, 6.377, 3.455);
            ctx.bcurve_to(6.066, 3.585, 5.713, 3.384, 5.609, 3.064);
            ctx.bcurve_to(5.504, 2.743, 5.673, 2.372, 6.000, 2.295);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(6.581, 6.176);
            ctx.bcurve_to(6.659, 6.364, 6.851, 6.475, 7.044, 6.413);
            ctx.bcurve_to(7.237, 6.349, 7.327, 6.147, 7.280, 5.949);
            ctx.qcurve_to(6.935, 4.494, 6.590, 3.039);
            ctx.bcurve_to(6.511, 2.711, 6.141, 2.543, 5.821, 2.647);
            ctx.bcurve_to(5.501, 2.751, 5.299, 3.105, 5.430, 3.416);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-2.060, 9.601);
            ctx.bcurve_to(-2.093, 9.869, -1.921, 10.117, -1.655, 10.158);
            ctx.bcurve_to(-1.389, 10.200, -1.149, 10.018, -1.099, 9.754);
            ctx.qcurve_to(-0.183, 5.000, 0.733, 0.246);
            ctx.bcurve_to(0.818, -0.193, 0.502, -0.609, 0.061, -0.679);
            ctx.bcurve_to(-0.379, -0.749, -0.808, -0.450, -0.863, -0.007);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-4.132, 4.801);
            ctx.bcurve_to(-4.336, 4.977, -4.397, 5.264, -4.239, 5.481);
            ctx.bcurve_to(-4.081, 5.699, -3.789, 5.729, -3.559, 5.589);
            ctx.qcurve_to(-1.867, 4.558, -0.175, 3.528);
            ctx.bcurve_to(0.206, 3.296, 0.266, 2.760, 0.003, 2.399);
            ctx.bcurve_to(-0.259, 2.038, -0.786, 1.929, -1.125, 2.220);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(2.334, 5.826);
            ctx.bcurve_to(2.473, 6.055, 2.443, 6.347, 2.226, 6.505);
            ctx.bcurve_to(2.008, 6.664, 1.721, 6.602, 1.546, 6.398);
            ctx.qcurve_to(0.255, 4.895, -1.035, 3.392);
            ctx.bcurve_to(-1.327, 3.053, -1.218, 2.525, -0.857, 2.263);
            ctx.bcurve_to(-0.496, 2.000, 0.040, 2.061, 0.272, 2.441);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(-3.947, 8.155);
            ctx.bcurve_to(-4.101, 8.288, -4.147, 8.505, -4.028, 8.669);
            ctx.bcurve_to(-3.909, 8.833, -3.689, 8.855, -3.515, 8.750);
            ctx.qcurve_to(-2.238, 7.972, -0.960, 7.194);
            ctx.bcurve_to(-0.673, 7.019, -0.627, 6.615, -0.826, 6.342);
            ctx.bcurve_to(-1.023, 6.069, -1.422, 5.987, -1.678, 6.207);
            ctx.fill();

            ctx.set_fill(0xff21853c);
            ctx.begin_path();
            ctx.move_to(1.121, 8.958);
            ctx.bcurve_to(1.227, 9.132, 1.204, 9.352, 1.040, 9.472);
            ctx.bcurve_to(0.875, 9.591, 0.659, 9.545, 0.527, 9.390);
            ctx.qcurve_to(-0.447, 8.256, -1.422, 7.121);
            ctx.bcurve_to(-1.641, 6.865, -1.559, 6.467, -1.287, 6.269);
            ctx.bcurve_to(-1.014, 6.071, -0.610, 6.116, -0.434, 6.404);
            ctx.fill();
            break;
        case MobID::kMantis:
            ctx.scale(radius / 10.0);
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(1.72);
            ctx.round_line_cap();
            #define draw_leg(cx, cy, ex, ey, anim) { \
                RenderContext context(&ctx); \
                ctx.translate(-2.867, 0); \
                ctx.rotate(anim); \
                ctx.translate(2.867, 0); \
                ctx.begin_path(); \
                ctx.move_to(-2.867, 0); \
                ctx.qcurve_to(cx, cy, ex, ey); \
                ctx.stroke(); \
            }
            draw_leg(-10.726, -4.046, -12.691,  -8.092,  sinf(attr.animation) * 0.15)
            draw_leg( -4.389, -6.292,  -4.770, -12.585,  cosf(attr.animation) * 0.15)
            draw_leg(  3.023, -5.191,   4.495, -10.383, -sinf(attr.animation) * 0.15)
            draw_leg( -7.909,  5.529,  -9.171,  11.058, -sinf(attr.animation) * 0.15)
            draw_leg( -6.036,  6.048,  -6.829,  12.096, -cosf(attr.animation) * 0.15)
            draw_leg(  4.318,  4.510,   6.114,   9.020,  sinf(attr.animation) * 0.15)
            #undef draw_leg

            ctx.set_fill(0xff78a62e);
            ctx.begin_path();
            ctx.move_to(4.207, 8.348);
            ctx.qcurve_to(-3.547, 11.175, -10.481, 8.598);
            ctx.qcurve_to(-18.429, 5.646, -18.429, 0.081);
            ctx.qcurve_to(-18.429, -5.483, -10.479, -8.435);
            ctx.qcurve_to(-3.547, -11.012, 4.207, -8.185);
            ctx.qcurve_to(11.330, -5.590, 11.330, 0.081);
            ctx.qcurve_to(11.330, 5.753, 4.206, 8.349);
            ctx.qcurve_to(4.207, 8.348, 4.207, 8.348);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(2.951, 4.901);
            ctx.qcurve_to(7.661, 3.185, 7.661, 0.082);
            ctx.qcurve_to(7.661, -3.021, 2.950, -4.738);
            ctx.qcurve_to(-3.534, -7.102, -9.202, -4.996);
            ctx.qcurve_to(-14.760, -2.931, -14.760, 0.081);
            ctx.qcurve_to(-14.760, 3.095, -9.202, 5.159);
            ctx.qcurve_to(-3.535, 7.265, 2.951, 4.901);
            ctx.fill();

            ctx.set_fill(0xff9acc46);
            ctx.begin_path();
            ctx.move_to(3.578, 6.625);
            ctx.bcurve_to(0.438, 7.770, -4.864, 8.729, -9.841, 6.879);
            ctx.bcurve_to(-14.819, 5.030, -16.595, 2.226, -16.595, 0.081);
            ctx.bcurve_to(-16.595, -2.063, -14.819, -4.867, -9.840, -6.715);
            ctx.bcurve_to(-4.864, -8.565, 0.438, -7.607, 3.579, -6.462);
            ctx.bcurve_to(6.719, -5.317, 9.496, -3.070, 9.496, 0.081);
            ctx.bcurve_to(9.495, 3.234, 6.720, 5.481, 3.579, 6.625);
            ctx.fill();

            ctx.set_fill(0xff78a62e);
            ctx.begin_path();
            ctx.move_to(-5.092, -4.312);
            ctx.qcurve_to(-6.930, -4.246, -8.410, -3.924);
            ctx.qcurve_to(-9.892, -3.601, -11.547, -2.908);
            ctx.qcurve_to(-11.588, -2.891, -11.632, -2.878);
            ctx.qcurve_to(-11.675, -2.865, -11.719, -2.855);
            ctx.qcurve_to(-11.763, -2.846, -11.808, -2.841);
            ctx.qcurve_to(-11.852, -2.837, -11.898, -2.837);
            ctx.qcurve_to(-11.943, -2.837, -11.988, -2.841);
            ctx.qcurve_to(-12.033, -2.845, -12.077, -2.854);
            ctx.qcurve_to(-12.121, -2.863, -12.165, -2.875);
            ctx.qcurve_to(-12.207, -2.889, -12.250, -2.905);
            ctx.qcurve_to(-12.291, -2.922, -12.331, -2.944);
            ctx.qcurve_to(-12.370, -2.965, -12.408, -2.989);
            ctx.qcurve_to(-12.445, -3.015, -12.481, -3.043);
            ctx.qcurve_to(-12.515, -3.071, -12.547, -3.103);
            ctx.qcurve_to(-12.579, -3.135, -12.608, -3.169);
            ctx.qcurve_to(-12.637, -3.204, -12.662, -3.242);
            ctx.qcurve_to(-12.687, -3.279, -12.709, -3.319);
            ctx.qcurve_to(-12.730, -3.358, -12.748, -3.400);
            ctx.qcurve_to(-12.765, -3.442, -12.778, -3.485);
            ctx.qcurve_to(-12.791, -3.527, -12.800, -3.572);
            ctx.qcurve_to(-12.810, -3.617, -12.814, -3.661);
            ctx.qcurve_to(-12.819, -3.705, -12.819, -3.751);
            ctx.qcurve_to(-12.819, -3.796, -12.815, -3.841);
            ctx.qcurve_to(-12.810, -3.886, -12.801, -3.930);
            ctx.qcurve_to(-12.793, -3.974, -12.780, -4.018);
            ctx.qcurve_to(-12.767, -4.060, -12.750, -4.102);
            ctx.qcurve_to(-12.733, -4.144, -12.712, -4.184);
            ctx.qcurve_to(-12.691, -4.223, -12.666, -4.261);
            ctx.qcurve_to(-12.641, -4.298, -12.613, -4.333);
            ctx.qcurve_to(-12.584, -4.368, -12.552, -4.401);
            ctx.qcurve_to(-12.520, -4.433, -12.486, -4.461);
            ctx.qcurve_to(-12.451, -4.490, -12.414, -4.515);
            ctx.qcurve_to(-12.376, -4.540, -12.337, -4.561);
            ctx.qcurve_to(-12.297, -4.583, -12.255, -4.600);
            ctx.qcurve_to(-10.446, -5.357, -8.799, -5.716);
            ctx.qcurve_to(-7.161, -6.073, -5.159, -6.145);
            ctx.qcurve_to(-5.114, -6.147, -5.069, -6.144);
            ctx.qcurve_to(-5.023, -6.141, -4.979, -6.133);
            ctx.qcurve_to(-4.935, -6.127, -4.891, -6.115);
            ctx.qcurve_to(-4.847, -6.104, -4.805, -6.088);
            ctx.qcurve_to(-4.763, -6.072, -4.723, -6.052);
            ctx.qcurve_to(-4.682, -6.032, -4.644, -6.009);
            ctx.qcurve_to(-4.606, -5.985, -4.569, -5.958);
            ctx.qcurve_to(-4.534, -5.931, -4.501, -5.899);
            ctx.qcurve_to(-4.467, -5.869, -4.437, -5.835);
            ctx.qcurve_to(-4.408, -5.802, -4.381, -5.765);
            ctx.qcurve_to(-4.355, -5.728, -4.333, -5.689);
            ctx.qcurve_to(-4.310, -5.651, -4.291, -5.609);
            ctx.qcurve_to(-4.273, -5.569, -4.258, -5.526);
            ctx.qcurve_to(-4.243, -5.483, -4.233, -5.439);
            ctx.qcurve_to(-4.223, -5.396, -4.216, -5.351);
            ctx.qcurve_to(-4.211, -5.306, -4.209, -5.261);
            ctx.qcurve_to(-4.207, -5.216, -4.210, -5.171);
            ctx.qcurve_to(-4.213, -5.126, -4.221, -5.082);
            ctx.qcurve_to(-4.227, -5.037, -4.239, -4.994);
            ctx.qcurve_to(-4.250, -4.950, -4.266, -4.908);
            ctx.qcurve_to(-4.282, -4.866, -4.302, -4.825);
            ctx.qcurve_to(-4.322, -4.784, -4.345, -4.747);
            ctx.qcurve_to(-4.369, -4.708, -4.396, -4.672);
            ctx.qcurve_to(-4.423, -4.637, -4.454, -4.603);
            ctx.qcurve_to(-4.485, -4.571, -4.519, -4.540);
            ctx.qcurve_to(-4.552, -4.510, -4.589, -4.484);
            ctx.qcurve_to(-4.626, -4.458, -4.665, -4.436);
            ctx.qcurve_to(-4.703, -4.412, -4.745, -4.394);
            ctx.qcurve_to(-4.785, -4.375, -4.828, -4.361);
            ctx.qcurve_to(-4.871, -4.346, -4.914, -4.336);
            ctx.qcurve_to(-4.958, -4.325, -5.003, -4.319);
            ctx.qcurve_to(-5.048, -4.313, -5.093, -4.311);
            ctx.fill();

            ctx.set_fill(0xff78a62e);
            ctx.begin_path();
            ctx.move_to(-5.158, 6.308);
            ctx.qcurve_to(-7.153, 6.237, -8.799, 5.880);
            ctx.qcurve_to(-10.449, 5.521, -12.256, 4.763);
            ctx.qcurve_to(-12.298, 4.745, -12.337, 4.724);
            ctx.qcurve_to(-12.378, 4.703, -12.415, 4.678);
            ctx.qcurve_to(-12.452, 4.653, -12.486, 4.623);
            ctx.qcurve_to(-12.521, 4.594, -12.553, 4.562);
            ctx.qcurve_to(-12.585, 4.531, -12.613, 4.496);
            ctx.qcurve_to(-12.642, 4.461, -12.666, 4.423);
            ctx.qcurve_to(-12.691, 4.386, -12.712, 4.346);
            ctx.qcurve_to(-12.733, 4.306, -12.751, 4.264);
            ctx.qcurve_to(-12.768, 4.223, -12.780, 4.180);
            ctx.qcurve_to(-12.793, 4.136, -12.802, 4.092);
            ctx.qcurve_to(-12.810, 4.048, -12.814, 4.003);
            ctx.qcurve_to(-12.819, 3.959, -12.819, 3.913);
            ctx.qcurve_to(-12.819, 3.868, -12.814, 3.823);
            ctx.qcurve_to(-12.810, 3.779, -12.800, 3.734);
            ctx.qcurve_to(-12.791, 3.690, -12.778, 3.647);
            ctx.qcurve_to(-12.765, 3.604, -12.748, 3.562);
            ctx.qcurve_to(-12.730, 3.520, -12.709, 3.481);
            ctx.qcurve_to(-12.687, 3.442, -12.661, 3.404);
            ctx.qcurve_to(-12.637, 3.367, -12.608, 3.332);
            ctx.qcurve_to(-12.578, 3.297, -12.547, 3.265);
            ctx.qcurve_to(-12.515, 3.233, -12.481, 3.205);
            ctx.qcurve_to(-12.446, 3.177, -12.408, 3.152);
            ctx.qcurve_to(-12.370, 3.127, -12.330, 3.106);
            ctx.qcurve_to(-12.291, 3.085, -12.249, 3.068);
            ctx.qcurve_to(-12.207, 3.050, -12.164, 3.039);
            ctx.qcurve_to(-12.121, 3.026, -12.076, 3.017);
            ctx.qcurve_to(-12.032, 3.008, -11.987, 3.004);
            ctx.qcurve_to(-11.943, 3.000, -11.897, 3.000);
            ctx.qcurve_to(-11.852, 3.000, -11.807, 3.005);
            ctx.qcurve_to(-11.763, 3.010, -11.718, 3.019);
            ctx.qcurve_to(-11.675, 3.028, -11.631, 3.041);
            ctx.qcurve_to(-11.588, 3.054, -11.546, 3.072);
            ctx.qcurve_to(-9.894, 3.765, -8.410, 4.087);
            ctx.qcurve_to(-6.924, 4.410, -5.093, 4.475);
            ctx.qcurve_to(-5.048, 4.477, -5.004, 4.482);
            ctx.qcurve_to(-4.959, 4.489, -4.915, 4.499);
            ctx.qcurve_to(-4.871, 4.509, -4.829, 4.524);
            ctx.qcurve_to(-4.786, 4.538, -4.745, 4.557);
            ctx.qcurve_to(-4.703, 4.576, -4.665, 4.599);
            ctx.qcurve_to(-4.626, 4.621, -4.590, 4.647);
            ctx.qcurve_to(-4.553, 4.674, -4.519, 4.703);
            ctx.qcurve_to(-4.485, 4.734, -4.454, 4.766);
            ctx.qcurve_to(-4.424, 4.799, -4.396, 4.835);
            ctx.qcurve_to(-4.369, 4.871, -4.345, 4.910);
            ctx.qcurve_to(-4.322, 4.948, -4.302, 4.988);
            ctx.qcurve_to(-4.282, 5.029, -4.266, 5.071);
            ctx.qcurve_to(-4.251, 5.113, -4.239, 5.156);
            ctx.qcurve_to(-4.228, 5.200, -4.220, 5.245);
            ctx.qcurve_to(-4.213, 5.289, -4.210, 5.334);
            ctx.qcurve_to(-4.207, 5.379, -4.209, 5.424);
            ctx.qcurve_to(-4.211, 5.469, -4.217, 5.514);
            ctx.qcurve_to(-4.222, 5.558, -4.233, 5.603);
            ctx.qcurve_to(-4.243, 5.646, -4.258, 5.689);
            ctx.qcurve_to(-4.273, 5.731, -4.291, 5.772);
            ctx.qcurve_to(-4.310, 5.814, -4.333, 5.852);
            ctx.qcurve_to(-4.355, 5.892, -4.381, 5.928);
            ctx.qcurve_to(-4.408, 5.964, -4.438, 5.998);
            ctx.qcurve_to(-4.467, 6.032, -4.500, 6.063);
            ctx.qcurve_to(-4.533, 6.093, -4.569, 6.121);
            ctx.qcurve_to(-4.605, 6.148, -4.644, 6.172);
            ctx.qcurve_to(-4.682, 6.195, -4.722, 6.216);
            ctx.qcurve_to(-4.763, 6.235, -4.804, 6.251);
            ctx.qcurve_to(-4.847, 6.266, -4.891, 6.278);
            ctx.qcurve_to(-4.934, 6.290, -4.979, 6.297);
            ctx.qcurve_to(-5.023, 6.304, -5.068, 6.307);
            ctx.qcurve_to(-5.113, 6.310, -5.158, 6.308);
            ctx.fill();

            ctx.set_fill(0xff78a62e);
            ctx.begin_path();
            ctx.move_to(1.941, -3.744);
            ctx.qcurve_to(0.791, -1.211, 0.791, 0.081);
            ctx.qcurve_to(0.791, 1.374, 1.940, 3.908);
            ctx.qcurve_to(1.960, 3.949, 1.974, 3.992);
            ctx.qcurve_to(1.988, 4.034, 1.999, 4.078);
            ctx.qcurve_to(2.009, 4.121, 2.015, 4.166);
            ctx.qcurve_to(2.020, 4.211, 2.022, 4.256);
            ctx.qcurve_to(2.024, 4.301, 2.021, 4.346);
            ctx.qcurve_to(2.018, 4.391, 2.011, 4.436);
            ctx.qcurve_to(2.003, 4.480, 1.992, 4.523);
            ctx.qcurve_to(1.980, 4.567, 1.964, 4.609);
            ctx.qcurve_to(1.948, 4.651, 1.928, 4.692);
            ctx.qcurve_to(1.909, 4.732, 1.885, 4.771);
            ctx.qcurve_to(1.861, 4.809, 1.833, 4.845);
            ctx.qcurve_to(1.806, 4.881, 1.775, 4.914);
            ctx.qcurve_to(1.744, 4.946, 1.711, 4.976);
            ctx.qcurve_to(1.676, 5.006, 1.640, 5.032);
            ctx.qcurve_to(1.604, 5.058, 1.565, 5.081);
            ctx.qcurve_to(1.525, 5.104, 1.484, 5.122);
            ctx.qcurve_to(1.443, 5.141, 1.401, 5.155);
            ctx.qcurve_to(1.358, 5.170, 1.314, 5.180);
            ctx.qcurve_to(1.270, 5.190, 1.226, 5.196);
            ctx.qcurve_to(1.181, 5.202, 1.136, 5.203);
            ctx.qcurve_to(1.091, 5.205, 1.046, 5.202);
            ctx.qcurve_to(1.001, 5.200, 0.956, 5.192);
            ctx.qcurve_to(0.912, 5.184, 0.868, 5.173);
            ctx.qcurve_to(0.825, 5.161, 0.782, 5.146);
            ctx.qcurve_to(0.741, 5.129, 0.700, 5.109);
            ctx.qcurve_to(0.660, 5.090, 0.621, 5.066);
            ctx.qcurve_to(0.583, 5.042, 0.548, 5.015);
            ctx.qcurve_to(0.511, 4.987, 0.479, 4.956);
            ctx.qcurve_to(0.445, 4.926, 0.416, 4.892);
            ctx.qcurve_to(0.386, 4.858, 0.360, 4.822);
            ctx.qcurve_to(0.334, 4.785, 0.311, 4.746);
            ctx.qcurve_to(0.288, 4.707, 0.270, 4.666);
            ctx.qcurve_to(-1.043, 1.771, -1.043, 0.081);
            ctx.qcurve_to(-1.043, -1.608, 0.270, -4.502);
            ctx.qcurve_to(0.288, -4.544, 0.311, -4.583);
            ctx.qcurve_to(0.333, -4.622, 0.360, -4.658);
            ctx.qcurve_to(0.386, -4.695, 0.416, -4.729);
            ctx.qcurve_to(0.446, -4.763, 0.479, -4.793);
            ctx.qcurve_to(0.511, -4.824, 0.548, -4.852);
            ctx.qcurve_to(0.583, -4.879, 0.621, -4.903);
            ctx.qcurve_to(0.660, -4.927, 0.700, -4.946);
            ctx.qcurve_to(0.740, -4.966, 0.783, -4.982);
            ctx.qcurve_to(0.825, -4.998, 0.869, -5.010);
            ctx.qcurve_to(0.912, -5.021, 0.956, -5.029);
            ctx.qcurve_to(1.001, -5.036, 1.046, -5.039);
            ctx.qcurve_to(1.091, -5.042, 1.136, -5.040);
            ctx.qcurve_to(1.181, -5.039, 1.226, -5.033);
            ctx.qcurve_to(1.270, -5.027, 1.314, -5.017);
            ctx.qcurve_to(1.358, -5.006, 1.400, -4.992);
            ctx.qcurve_to(1.443, -4.977, 1.484, -4.959);
            ctx.qcurve_to(1.525, -4.941, 1.564, -4.917);
            ctx.qcurve_to(1.604, -4.895, 1.640, -4.869);
            ctx.qcurve_to(1.677, -4.842, 1.711, -4.813);
            ctx.qcurve_to(1.744, -4.783, 1.775, -4.750);
            ctx.qcurve_to(1.806, -4.717, 1.833, -4.681);
            ctx.qcurve_to(1.861, -4.646, 1.884, -4.607);
            ctx.qcurve_to(1.909, -4.569, 1.928, -4.529);
            ctx.qcurve_to(1.948, -4.488, 1.964, -4.447);
            ctx.qcurve_to(1.980, -4.404, 1.992, -4.360);
            ctx.qcurve_to(2.003, -4.317, 2.010, -4.273);
            ctx.qcurve_to(2.018, -4.228, 2.021, -4.183);
            ctx.qcurve_to(2.024, -4.138, 2.022, -4.093);
            ctx.qcurve_to(2.020, -4.048, 2.015, -4.003);
            ctx.qcurve_to(2.009, -3.959, 1.999, -3.915);
            ctx.qcurve_to(1.988, -3.870, 1.974, -3.828);
            ctx.qcurve_to(1.959, -3.785, 1.941, -3.744);
            ctx.fill();

            ctx.set_fill(0xff78a62e);
            ctx.begin_path();
            ctx.move_to(-13.399, -0.917);
            ctx.qcurve_to(-6.824, -1.117, -0.249, -1.317);
            ctx.qcurve_to(-0.203, -1.319, -0.159, -1.316);
            ctx.qcurve_to(-0.113, -1.313, -0.069, -1.306);
            ctx.qcurve_to(-0.025, -1.298, 0.019, -1.287);
            ctx.qcurve_to(0.061, -1.275, 0.104, -1.258);
            ctx.qcurve_to(0.146, -1.242, 0.187, -1.222);
            ctx.qcurve_to(0.227, -1.202, 0.265, -1.179);
            ctx.qcurve_to(0.304, -1.155, 0.339, -1.127);
            ctx.qcurve_to(0.375, -1.100, 0.408, -1.069);
            ctx.qcurve_to(0.441, -1.038, 0.470, -1.004);
            ctx.qcurve_to(0.500, -0.970, 0.526, -0.933);
            ctx.qcurve_to(0.552, -0.897, 0.574, -0.858);
            ctx.qcurve_to(0.597, -0.819, 0.616, -0.777);
            ctx.qcurve_to(0.634, -0.736, 0.648, -0.694);
            ctx.qcurve_to(0.663, -0.651, 0.673, -0.607);
            ctx.qcurve_to(0.684, -0.563, 0.689, -0.518);
            ctx.qcurve_to(0.695, -0.473, 0.696, -0.429);
            ctx.qcurve_to(0.696, -0.415, 0.697, -0.401);
            ctx.qcurve_to(0.697, 0.000, 0.697, 0.402);
            ctx.qcurve_to(0.697, 0.447, 0.692, 0.491);
            ctx.qcurve_to(0.688, 0.536, 0.679, 0.580);
            ctx.qcurve_to(0.670, 0.625, 0.657, 0.667);
            ctx.qcurve_to(0.644, 0.711, 0.627, 0.753);
            ctx.qcurve_to(0.609, 0.794, 0.588, 0.834);
            ctx.qcurve_to(0.567, 0.873, 0.542, 0.911);
            ctx.qcurve_to(0.517, 0.948, 0.488, 0.983);
            ctx.qcurve_to(0.460, 1.018, 0.428, 1.050);
            ctx.qcurve_to(0.396, 1.082, 0.361, 1.111);
            ctx.qcurve_to(0.326, 1.140, 0.289, 1.164);
            ctx.qcurve_to(0.251, 1.189, 0.212, 1.210);
            ctx.qcurve_to(0.172, 1.231, 0.130, 1.249);
            ctx.qcurve_to(0.089, 1.266, 0.045, 1.279);
            ctx.qcurve_to(0.002, 1.292, -0.042, 1.301);
            ctx.qcurve_to(-0.086, 1.310, -0.131, 1.314);
            ctx.qcurve_to(-0.175, 1.319, -0.220, 1.319);
            ctx.qcurve_to(-0.235, 1.318, -0.249, 1.318);
            ctx.qcurve_to(-6.824, 1.117, -13.399, 0.916);
            ctx.qcurve_to(-13.444, 0.915, -13.488, 0.909);
            ctx.qcurve_to(-13.533, 0.904, -13.577, 0.894);
            ctx.qcurve_to(-13.620, 0.883, -13.663, 0.869);
            ctx.qcurve_to(-13.706, 0.855, -13.747, 0.836);
            ctx.qcurve_to(-13.788, 0.818, -13.827, 0.795);
            ctx.qcurve_to(-13.866, 0.772, -13.903, 0.747);
            ctx.qcurve_to(-13.940, 0.720, -13.973, 0.691);
            ctx.qcurve_to(-14.008, 0.661, -14.039, 0.628);
            ctx.qcurve_to(-14.069, 0.595, -14.097, 0.560);
            ctx.qcurve_to(-14.125, 0.524, -14.148, 0.486);
            ctx.qcurve_to(-14.172, 0.448, -14.192, 0.407);
            ctx.qcurve_to(-14.212, 0.367, -14.228, 0.325);
            ctx.qcurve_to(-14.244, 0.282, -14.256, 0.239);
            ctx.qcurve_to(-14.268, 0.196, -14.275, 0.151);
            ctx.qcurve_to(-14.283, 0.107, -14.285, 0.062);
            ctx.qcurve_to(-14.289, 0.017, -14.287, -0.028);
            ctx.qcurve_to(-14.286, -0.072, -14.281, -0.114);
            ctx.qcurve_to(-14.275, -0.158, -14.266, -0.200);
            ctx.qcurve_to(-14.257, -0.242, -14.243, -0.284);
            ctx.qcurve_to(-14.230, -0.324, -14.213, -0.364);
            ctx.qcurve_to(-14.195, -0.404, -14.174, -0.441);
            ctx.qcurve_to(-14.154, -0.480, -14.129, -0.516);
            ctx.qcurve_to(-14.105, -0.552, -14.077, -0.585);
            ctx.qcurve_to(-14.050, -0.618, -14.019, -0.649);
            ctx.qcurve_to(-13.988, -0.680, -13.955, -0.708);
            ctx.qcurve_to(-13.922, -0.735, -13.886, -0.760);
            ctx.qcurve_to(-13.850, -0.783, -13.812, -0.804);
            ctx.qcurve_to(-13.774, -0.826, -13.735, -0.843);
            ctx.qcurve_to(-13.694, -0.860, -13.654, -0.873);
            ctx.qcurve_to(-13.612, -0.886, -13.570, -0.896);
            ctx.qcurve_to(-13.528, -0.905, -13.485, -0.911);
            ctx.qcurve_to(-13.441, -0.916, -13.399, -0.917);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(-13.343, 0.916);
            ctx.qcurve_to(-13.357, 0.458, -13.371, -0.001);
            ctx.qcurve_to(-13.357, -0.459, -13.343, -0.917);
            ctx.qcurve_to(-6.768, -0.717, -0.193, -0.516);
            ctx.qcurve_to(-0.207, -0.058, -0.221, 0.401);
            ctx.qcurve_to(-0.679, 0.401, -1.138, 0.400);
            ctx.qcurve_to(-1.138, -0.001, -1.138, -0.402);
            ctx.qcurve_to(-0.679, -0.402, -0.221, -0.401);
            ctx.qcurve_to(-0.207, 0.057, -0.192, 0.515);
            ctx.qcurve_to(-6.768, 0.716, -13.343, 0.916);
            ctx.fill();

            ctx.set_fill(0xff353535);
            ctx.begin_path();
            ctx.move_to(7.275, -3.295);
            ctx.qcurve_to(11.314, -3.754, 14.052, -5.357);
            ctx.qcurve_to(16.797, -6.964, 19.337, -10.361);
            ctx.qcurve_to(19.337, -10.361, 19.337, -10.360);
            ctx.qcurve_to(19.644, -10.131, 19.951, -9.901);
            ctx.qcurve_to(20.178, -9.591, 20.404, -9.281);
            ctx.qcurve_to(16.000, -6.063, 14.296, -5.066);
            ctx.qcurve_to(12.593, -4.069, 7.680, -1.835);
            ctx.qcurve_to(7.521, -2.184, 7.362, -2.533);
            ctx.qcurve_to(7.319, -2.914, 7.275, -3.295);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(7.448, -1.771);
            ctx.qcurve_to(7.389, -1.764, 7.328, -1.767);
            ctx.qcurve_to(7.268, -1.769, 7.208, -1.781);
            ctx.qcurve_to(7.149, -1.794, 7.093, -1.815);
            ctx.qcurve_to(7.036, -1.836, 6.984, -1.865);
            ctx.qcurve_to(6.931, -1.895, 6.884, -1.933);
            ctx.qcurve_to(6.837, -1.971, 6.796, -2.015);
            ctx.qcurve_to(6.755, -2.059, 6.722, -2.110);
            ctx.qcurve_to(6.688, -2.160, 6.664, -2.215);
            ctx.qcurve_to(6.648, -2.250, 6.636, -2.286);
            ctx.qcurve_to(6.624, -2.321, 6.615, -2.358);
            ctx.qcurve_to(6.607, -2.394, 6.601, -2.432);
            ctx.qcurve_to(6.597, -2.470, 6.595, -2.507);
            ctx.qcurve_to(6.594, -2.544, 6.596, -2.582);
            ctx.qcurve_to(6.598, -2.619, 6.605, -2.657);
            ctx.qcurve_to(6.611, -2.694, 6.621, -2.731);
            ctx.qcurve_to(6.631, -2.767, 6.644, -2.802);
            ctx.qcurve_to(6.657, -2.838, 6.673, -2.872);
            ctx.qcurve_to(6.690, -2.905, 6.710, -2.937);
            ctx.qcurve_to(6.730, -2.970, 6.753, -2.999);
            ctx.qcurve_to(6.776, -3.030, 6.801, -3.057);
            ctx.qcurve_to(6.827, -3.085, 6.855, -3.109);
            ctx.qcurve_to(6.884, -3.135, 6.914, -3.156);
            ctx.qcurve_to(6.945, -3.178, 6.977, -3.197);
            ctx.qcurve_to(7.010, -3.216, 7.044, -3.232);
            ctx.qcurve_to(11.886, -5.433, 13.521, -6.390);
            ctx.qcurve_to(15.158, -7.348, 19.498, -10.521);
            ctx.qcurve_to(19.548, -10.557, 19.602, -10.584);
            ctx.qcurve_to(19.658, -10.612, 19.715, -10.631);
            ctx.qcurve_to(19.774, -10.650, 19.834, -10.659);
            ctx.qcurve_to(19.894, -10.669, 19.955, -10.668);
            ctx.qcurve_to(20.017, -10.668, 20.077, -10.658);
            ctx.qcurve_to(20.138, -10.648, 20.195, -10.629);
            ctx.qcurve_to(20.253, -10.609, 20.307, -10.581);
            ctx.qcurve_to(20.362, -10.552, 20.411, -10.515);
            ctx.qcurve_to(20.441, -10.493, 20.469, -10.467);
            ctx.qcurve_to(20.497, -10.442, 20.522, -10.415);
            ctx.qcurve_to(20.547, -10.386, 20.569, -10.356);
            ctx.qcurve_to(20.592, -10.325, 20.611, -10.293);
            ctx.qcurve_to(20.630, -10.261, 20.646, -10.227);
            ctx.qcurve_to(20.662, -10.192, 20.675, -10.156);
            ctx.qcurve_to(20.688, -10.121, 20.697, -10.085);
            ctx.qcurve_to(20.705, -10.048, 20.711, -10.010);
            ctx.qcurve_to(20.716, -9.973, 20.718, -9.936);
            ctx.qcurve_to(20.720, -9.898, 20.718, -9.860);
            ctx.qcurve_to(20.716, -9.823, 20.710, -9.786);
            ctx.qcurve_to(20.704, -9.749, 20.695, -9.712);
            ctx.qcurve_to(20.686, -9.675, 20.673, -9.639);
            ctx.qcurve_to(20.661, -9.604, 20.644, -9.570);
            ctx.qcurve_to(20.627, -9.536, 20.608, -9.504);
            ctx.qcurve_to(20.588, -9.472, 20.566, -9.442);
            ctx.qcurve_to(20.566, -9.442, 20.566, -9.441);
            ctx.qcurve_to(17.840, -5.796, 14.828, -4.032);
            ctx.qcurve_to(11.809, -2.265, 7.448, -1.771);
            ctx.fill();

            ctx.set_fill(0xff353535);
            ctx.begin_path();
            ctx.move_to(7.449, 1.934);
            ctx.qcurve_to(11.809, 2.429, 14.827, 4.196);
            ctx.qcurve_to(17.840, 5.959, 20.566, 9.604);
            ctx.qcurve_to(20.566, 9.605, 20.566, 9.605);
            ctx.qcurve_to(20.588, 9.635, 20.608, 9.667);
            ctx.qcurve_to(20.627, 9.700, 20.644, 9.733);
            ctx.qcurve_to(20.660, 9.767, 20.673, 9.803);
            ctx.qcurve_to(20.686, 9.838, 20.695, 9.875);
            ctx.qcurve_to(20.704, 9.912, 20.710, 9.949);
            ctx.qcurve_to(20.716, 9.986, 20.718, 10.024);
            ctx.qcurve_to(20.719, 10.062, 20.718, 10.099);
            ctx.qcurve_to(20.716, 10.136, 20.711, 10.173);
            ctx.qcurve_to(20.706, 10.211, 20.697, 10.248);
            ctx.qcurve_to(20.687, 10.284, 20.675, 10.320);
            ctx.qcurve_to(20.662, 10.356, 20.646, 10.390);
            ctx.qcurve_to(20.630, 10.424, 20.611, 10.456);
            ctx.qcurve_to(20.592, 10.488, 20.569, 10.519);
            ctx.qcurve_to(20.547, 10.549, 20.522, 10.577);
            ctx.qcurve_to(20.497, 10.606, 20.469, 10.630);
            ctx.qcurve_to(20.441, 10.657, 20.411, 10.679);
            ctx.qcurve_to(20.362, 10.716, 20.307, 10.744);
            ctx.qcurve_to(20.253, 10.772, 20.196, 10.792);
            ctx.qcurve_to(20.138, 10.811, 20.077, 10.821);
            ctx.qcurve_to(20.017, 10.831, 19.955, 10.831);
            ctx.qcurve_to(19.894, 10.831, 19.834, 10.823);
            ctx.qcurve_to(19.774, 10.813, 19.715, 10.795);
            ctx.qcurve_to(19.657, 10.776, 19.602, 10.747);
            ctx.qcurve_to(19.547, 10.720, 19.498, 10.684);
            ctx.qcurve_to(15.158, 7.511, 13.521, 6.553);
            ctx.qcurve_to(11.895, 5.601, 7.045, 3.395);
            ctx.qcurve_to(7.045, 3.395, 7.044, 3.394);
            ctx.qcurve_to(7.010, 3.379, 6.977, 3.360);
            ctx.qcurve_to(6.945, 3.341, 6.914, 3.320);
            ctx.qcurve_to(6.884, 3.298, 6.855, 3.273);
            ctx.qcurve_to(6.827, 3.248, 6.801, 3.220);
            ctx.qcurve_to(6.776, 3.193, 6.753, 3.162);
            ctx.qcurve_to(6.730, 3.132, 6.710, 3.100);
            ctx.qcurve_to(6.689, 3.069, 6.673, 3.034);
            ctx.qcurve_to(6.658, 3.000, 6.644, 2.966);
            ctx.qcurve_to(6.631, 2.930, 6.621, 2.894);
            ctx.qcurve_to(6.611, 2.857, 6.605, 2.820);
            ctx.qcurve_to(6.598, 2.783, 6.596, 2.746);
            ctx.qcurve_to(6.594, 2.708, 6.595, 2.671);
            ctx.qcurve_to(6.597, 2.633, 6.601, 2.595);
            ctx.qcurve_to(6.607, 2.558, 6.615, 2.521);
            ctx.qcurve_to(6.624, 2.484, 6.636, 2.449);
            ctx.qcurve_to(6.648, 2.413, 6.664, 2.378);
            ctx.qcurve_to(6.688, 2.324, 6.721, 2.273);
            ctx.qcurve_to(6.755, 2.223, 6.796, 2.179);
            ctx.qcurve_to(6.837, 2.134, 6.884, 2.096);
            ctx.qcurve_to(6.931, 2.058, 6.984, 2.028);
            ctx.qcurve_to(7.036, 1.999, 7.093, 1.978);
            ctx.qcurve_to(7.149, 1.957, 7.209, 1.945);
            ctx.qcurve_to(7.268, 1.932, 7.328, 1.930);
            ctx.qcurve_to(7.389, 1.927, 7.449, 1.934);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(7.275, 3.459);
            ctx.qcurve_to(7.319, 3.078, 7.362, 2.696);
            ctx.qcurve_to(7.521, 2.347, 7.680, 1.997);
            ctx.qcurve_to(7.680, 1.998, 7.680, 1.998);
            ctx.qcurve_to(12.603, 4.238, 14.296, 5.229);
            ctx.qcurve_to(16.000, 6.226, 20.404, 9.444);
            ctx.qcurve_to(20.178, 9.754, 19.952, 10.064);
            ctx.qcurve_to(19.644, 10.294, 19.337, 10.523);
            ctx.qcurve_to(19.337, 10.524, 19.337, 10.524);
            ctx.qcurve_to(16.797, 7.127, 14.052, 5.520);
            ctx.qcurve_to(11.313, 3.917, 7.275, 3.459);
            ctx.fill();
            break;
        default:
            assert(!"Didn't cover mob render");
            break;
    }
}