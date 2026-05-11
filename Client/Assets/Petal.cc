#include <Client/Assets/Assets.hh>

#include <Client/StaticData.hh>

#include <Shared/Helpers.hh>
#include <Shared/StaticData.hh>

#include <cmath>
#include <cstring>

void draw_static_petal_single(PetalID::T id, Renderer &ctx) {
    float r = PETAL_DATA[id].radius;
    switch(id) {
        case PetalID::kNone:
            break;
        case PetalID::kDandelion:
            ctx.set_stroke(0xff222222);
            ctx.round_line_cap();
            ctx.set_line_width(7);
            ctx.begin_path();
            ctx.move_to(0,0);
            ctx.line_to(-1.6 * r, 0);
            ctx.stroke();
        case PetalID::kUniqueBasic:
        case PetalID::kBasic:
        case PetalID::kUnusualBasic:
        case PetalID::kRareBasic:
        case PetalID::kEpicBasic:
        case PetalID::kLight:
        case PetalID::kUnusualLight:
        case PetalID::kRareLight:
        case PetalID::kEpicLight:
        case PetalID::kTwin:
        case PetalID::kTriplet:
            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xffcfcfcf);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kHeavy:
        case PetalID::kUnusualHeavy:
        case PetalID::kRareHeavy:
        case PetalID::kEpicHeavy:
        case PetalID::kLegendaryHeavy:
            ctx.set_fill(0xffaaaaaa);
            ctx.set_stroke(0xff888888);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kCommonStinger:
        case PetalID::kRareStinger:
        case PetalID::kEpicStinger:
        case PetalID::kMythicTringer:
        case PetalID::kStinger: {
        case PetalID::kTringer:
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(r,0);
            ctx.line_to(-r*0.5,r*0.866);
            ctx.line_to(-r*0.5,-r*0.866);
            ctx.line_to(r,0);
            ctx.fill();
            ctx.stroke();
            break;
        }
        case PetalID::kCommonLeaf:
        case PetalID::kRareLeaf:
        case PetalID::kEpicLeaf:
        case PetalID::kLegendaryLeaf:
        case PetalID::kLeaf:
            ctx.set_fill(0xff39b54a);
            ctx.set_stroke(0xff2e933c);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(-20, 0);
            ctx.line_to(-15, 0);
            ctx.bcurve_to(-10,-12,5,-12,15,0);
            ctx.bcurve_to(5,12,-10,12,-15,0);
            ctx.fill();
            ctx.stroke();
            ctx.begin_path();
            ctx.move_to(-9,0);
            ctx.qcurve_to(0,-1.5,7.5,0);
            ctx.stroke();
            break;
        case PetalID::kCommonRose:
        case PetalID::kRareRose:
        case PetalID::kLegendaryRose:
        case PetalID::kRose:
        case PetalID::kDahlia:
            ctx.set_fill(0xffff94c9);
            ctx.set_stroke(0xffcf78a3);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kAntEgg:
            ctx.set_stroke(0xffcfc295);
            ctx.set_fill(0xfffff0b8);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kBeetleEgg:
            ctx.begin_path();
            ctx.ellipse(0,0,r * 0.85, r * 1.15);
            ctx.set_fill(0xfffff0b8);
            ctx.fill();
            ctx.set_stroke(0xffcfc295);
            ctx.set_line_width(3);
            ctx.stroke();
            break;
        case PetalID::kMissile:
            ctx.scale(r / 10);
            ctx.set_fill(0xff222222);
            ctx.set_stroke(0xff222222);
            ctx.set_line_width(5.0);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(11.0, 0.0);
            ctx.line_to(-11.0, -6.0);
            ctx.line_to(-11.0, 6.0);
            ctx.line_to(11.0, 0.0);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kCommonIris:
        case PetalID::kRareIris:
        case PetalID::kLegendaryIris:
        case PetalID::kIris:
            ctx.set_fill(0xffce76db);
            ctx.set_stroke(0xffa760b1);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kPollen:
            ctx.set_fill(0xffffe763);
            ctx.set_stroke(0xffcfbb50);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kCommonBubble:
        case PetalID::kUnusualBubble:
        case PetalID::kEpicBubble:
        case PetalID::kLegendaryBubble:
        case PetalID::kBubble:
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.set_stroke(0xb2ffffff);
            ctx.set_line_width(3);
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(0,0,r-1.5);
            ctx.set_fill(0x59ffffff);
            ctx.fill();
            ctx.begin_path();
            ctx.arc(r/3,-r/3,r/4);
            ctx.set_fill(0x59ffffff);
            ctx.fill();
            break;
        case PetalID::kFaster:
            ctx.set_fill(0xfffeffc9);
            ctx.set_stroke(0xffcecfa3);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kThirdEye:
            ctx.scale(0.5);
            ctx.set_fill(0xff111111);
            ctx.begin_path();
            ctx.move_to(0,-10);
            ctx.qcurve_to(8,0,0,10);
            ctx.qcurve_to(-8,0,0,-10);
            ctx.fill();
            ctx.set_fill(0xffeeeeee);
            ctx.begin_path();
            ctx.arc(0, 0, 5);
            ctx.fill();
            ctx.set_stroke(0xff111111);
            ctx.set_line_width(1.5);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(0,-10);
            ctx.qcurve_to(8,0,0,10);
            ctx.qcurve_to(-8,0,0,-10);
            ctx.stroke();
            break;
        case PetalID::kWeb:
        case PetalID::kTriweb:
            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xffcfcfcf);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(11.00, 0.00);
            ctx.qcurve_to(4.32, 3.14, 3.40, 10.46);
            ctx.qcurve_to(-1.65, 5.08, -8.90, 6.47);
            ctx.qcurve_to(-5.34, -0.00, -8.90, -6.47);
            ctx.qcurve_to(-1.65, -5.08, 3.40, -10.46);
            ctx.qcurve_to(4.32, -3.14, 11.00, 0.00);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kWing:
            ctx.begin_path();
            ctx.partial_arc(0,0,15,-1.5707963267948966,1.5707963267948966,0);
            ctx.qcurve_to(10,0,0,-15);
            ctx.set_fill(0xffffffff);
            ctx.fill();
            ctx.set_stroke(0xffcfcfcf);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.stroke();
            break;
        case PetalID::kCommonRock:
        case PetalID::kUnusualRock:
        case PetalID::kEpicRock:
        case PetalID::kLegendaryRock:
        case PetalID::kRock: {
            ctx.set_fill(0xff777777);
            ctx.set_stroke(Renderer::HSV(0xff777777, 0.8));
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(12.138091087341309,0);
            ctx.line_to(3.8414306640625,12.377452850341797);
            ctx.line_to(-11.311542510986328,7.916932582855225);
            ctx.line_to(-11.461170196533203,-7.836822032928467);
            ctx.line_to(4.538298606872559,-13.891617774963379);
            ctx.line_to(12.138091087341309,0);
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            break;
        }
        case PetalID::kAntennae: {
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(5, 12.5);
            ctx.qcurve_to(10, -2.5, 15, -12.5);
            ctx.qcurve_to(5, -2.5, 5, 12.5);
            ctx.move_to(-5, 12.5);
            ctx.qcurve_to(-10, -2.5, -15, -12.5);
            ctx.qcurve_to(-5, -2.5, -5, 12.5);
            ctx.fill();
            ctx.stroke();
            break;
        }
        case PetalID::kObserver: {
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_stroke(0xff333333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(5, 12.5);
            ctx.qcurve_to(10, -2.5, 15, -12.5);
            ctx.qcurve_to(5, -2.5, 5, 12.5);
            ctx.move_to(-5, 12.5);
            ctx.qcurve_to(-10, -2.5, -15, -12.5);
            ctx.qcurve_to(-5, -2.5, -5, 12.5);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0xffd01c1d);
            ctx.begin_path();
            ctx.arc(15, -12.5, 2.5);
            ctx.close_path();
            ctx.arc(-15, -12.5, 2.5);
            ctx.close_path();
            ctx.fill();
            break;
        }
        case PetalID::kBlueIris:
            ctx.set_fill(0xff39e9f1);
            ctx.set_stroke(0xff2dbac0);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kCactus:
        case PetalID::kTricac:
            ctx.set_fill(0xff38c75f);
            ctx.set_stroke(Renderer::HSV(0xff38c75f, 0.8));
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(15,0);
            for (uint32_t i = 0; i < 8; ++i) {
                float base_angle = M_PI * 2 * i / 8;
                ctx.qcurve_to(15*0.8*cosf(base_angle+M_PI/8),15*0.8*sinf(base_angle+M_PI/8),15*cosf(base_angle+2*M_PI/8),15*sinf(base_angle+2*M_PI/8));
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0xff74d68f);
            ctx.begin_path();
            ctx.arc(0,0,8);
            ctx.fill();
            break;
        case PetalID::kPoisonPeas:
            ctx.set_fill(0xffce76db);
            ctx.set_stroke(0xffa760b1);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,PETAL_DATA[id].radius);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kPeas:
            ctx.set_fill(0xff8ac255);
            ctx.set_stroke(0xff709d45);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.arc(0,0,PETAL_DATA[id].radius);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kSand:
            ctx.set_fill(0xffe0c85c);
            ctx.set_stroke(0xffb5a24b);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(7,0);
            ctx.line_to(3.499999761581421,6.062178134918213);
            ctx.line_to(-3.500000476837158,6.062177658081055);
            ctx.line_to(-7,-6.119594218034763e-7);
            ctx.line_to(-3.4999992847442627,-6.062178134918213);
            ctx.line_to(3.4999992847442627,-6.062178134918213);
            ctx.close_path();
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kStick:
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.set_line_width(7);
            ctx.set_stroke(0xff654a19);
            ctx.begin_path();
            ctx.move_to(0,10);
            ctx.line_to(0,0);
            ctx.line_to(4,-7);
            ctx.move_to(0,0);
            ctx.line_to(-6,-10);
            ctx.stroke();
            ctx.set_line_width(3);
            ctx.set_stroke(0xff7d5b1f);
            ctx.stroke();
            break;
        case PetalID::kPincer:
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(10,5);
            ctx.qcurve_to(4,-14,-10,5);
            ctx.qcurve_to(4,0,10,5);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kAzalea: {
            ctx.set_fill(0xffff94c9);
            ctx.set_stroke(0xffcf78a3);
            ctx.set_line_width(3);
            ctx.begin_path();
            uint32_t s = 3;
            ctx.move_to(r, 0);
            for (uint32_t i = 1; i <= s; ++i) {
                float angle = i * 2 * M_PI / s;
                float angle2 = angle - M_PI / s;
                ctx.qcurve_to(2 * r * cosf(angle2), 2 * r * sinf(angle2), r * cosf(angle), r * sinf(angle));
            }
            ctx.fill();
            ctx.stroke();
            break;
        }
        case PetalID::kPoisonCactus:
            ctx.set_fill(0xffce76db);
            ctx.set_stroke(0xffa760b1);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(15,0);
            for (uint32_t i = 0; i < 8; ++i) {
                float base_angle = M_PI * 2 * i / 8;
                ctx.qcurve_to(15*0.8*cosf(base_angle+M_PI/8),15*0.8*sinf(base_angle+M_PI/8),15*cosf(base_angle+2*M_PI/8),15*sinf(base_angle+2*M_PI/8));
            }
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0xffcea0db);
            ctx.begin_path();
            ctx.arc(0,0,8);
            ctx.fill();
            break;
        case PetalID::kSalt:
            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xffcfcfcf);
            ctx.set_line_width(3);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(10.404077529907227,0);
            ctx.line_to(6.643442630767822,8.721502304077148);
            ctx.line_to(-2.6667866706848145,11.25547981262207);
            ctx.line_to(-10.940428733825684,4.95847225189209);
            ctx.line_to(-11.341578483581543,-5.432167053222656);
            ctx.line_to(-2.4972469806671143,-11.472168922424316);
            ctx.line_to(7.798409461975098,-9.584606170654297);
            ctx.line_to(10.404077529907227,0);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kSquare:
            ctx.set_fill(0xffffe869);
            ctx.set_stroke(0xffcfbc55);
            ctx.set_line_width(0.15*r);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.rect(-r * 0.707, -r * 0.707, r * 1.414, r * 1.414);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kMoon: {
            ctx.set_fill(0xff878787);
            ctx.set_stroke(0xff6d6d6d);
            ctx.set_line_width(5);
            ctx.begin_path();
            ctx.arc(0,0,r);
            ctx.stroke();
            ctx.fill();
            ctx.clip();
            SeedGenerator gen(id * 274633 + 284562);
            uint32_t ct = 10;
            ctx.set_fill(0xff999999);
            ctx.set_stroke(0xff7c7c7c);
            ctx.set_line_width(3);
            ctx.begin_path();
            for (uint32_t i = 0; i < ct; ++i) {
                float _x = gen.binext() * (r + 10);
                float _y = gen.binext() * (r + 10);
                float _r = gen.binext() * 10 + 10;
                ctx.move_to(_x,_y);
                ctx.arc(_x,_y,_r);
            }
            ctx.stroke();
            ctx.fill(1);
            break;
        }
        case PetalID::kLotus:
            ctx.scale(r / 10);
            ctx.set_fill(0xffce76db);
            ctx.set_stroke(0xffa760b1);
            ctx.set_line_width(2);
            ctx.round_line_cap();
            ctx.round_line_join();
            ctx.begin_path();
            ctx.move_to(0.00, -10.00);
            ctx.bcurve_to(1.44, -7.11, 2.05, -5.89, 1.83, -4.41);
            ctx.bcurve_to(2.72, -5.62, 4.01, -6.05, 7.07, -7.07);
            ctx.bcurve_to(6.05, -4.01, 5.62, -2.72, 4.41, -1.83);
            ctx.bcurve_to(5.89, -2.05, 7.11, -1.44, 10.00, 0.00);
            ctx.bcurve_to(7.11, 1.44, 5.89, 2.05, 4.41, 1.83);
            ctx.bcurve_to(5.62, 2.72, 6.05, 4.01, 7.07, 7.07);
            ctx.bcurve_to(4.01, 6.05, 2.72, 5.62, 1.83, 4.41);
            ctx.bcurve_to(2.05, 5.89, 1.44, 7.11, 0.00, 10.00);
            ctx.bcurve_to(-1.44, 7.11, -2.05, 5.89, -1.83, 4.41);
            ctx.bcurve_to(-2.72, 5.62, -4.01, 6.05, -7.07, 7.07);
            ctx.bcurve_to(-6.05, 4.01, -5.62, 2.72, -4.41, 1.83);
            ctx.bcurve_to(-5.89, 2.05, -7.11, 1.44, -10.00, 0.00);
            ctx.bcurve_to(-7.11, -1.44, -5.89, -2.05, -4.41, -1.83);
            ctx.bcurve_to(-5.62, -2.72, -6.05, -4.01, -7.07, -7.07);
            ctx.bcurve_to(-4.01, -6.05, -2.72, -5.62, -1.83, -4.41);
            ctx.bcurve_to(-2.05, -5.89, -1.44, -7.11, 0.00, -10.00);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0xffa760b1);
            ctx.begin_path();
            ctx.arc(0,0,1.74);
            ctx.fill();
            break;
        case PetalID::kHeaviest:
            ctx.begin_path();
            ctx.arc(0,0,16);
            ctx.set_fill(0xff333333);
            ctx.fill();
            ctx.set_stroke(0xff292929);
            ctx.set_line_width(3);
            ctx.stroke();
            ctx.begin_path();
            ctx.arc(6,-6,4.6);
            ctx.set_fill(0xffcccccc);
            ctx.fill();
            break;
        case PetalID::kCutter:
            ctx.set_fill(0xff111111);
            ctx.begin_path();
            ctx.arc(0,0,25);
            ctx.move_to(24.748737335205078,24.748737335205078);
            ctx.qcurve_to(9.899494171142578,23.899494171142578,-0.0000015298985545086907,35);
            ctx.qcurve_to(-9.899496078491211,23.899494171142578,-24.748737335205078,24.748737335205078);
            ctx.qcurve_to(-23.899494171142578,9.899493217468262,-35,-0.0000030597971090173814);
            ctx.qcurve_to(-23.89949607849121,-9.899496078491211,-24.74873924255371,-24.748735427856445);
            ctx.qcurve_to(-9.899496078491211,-23.899494171142578,4.173708134658227e-7,-35);
            ctx.qcurve_to(9.899493217468262,-23.89949607849121,24.748733520507812,-24.748741149902344);
            ctx.qcurve_to(23.899494171142578,-9.899494171142578,35,0.000006119594218034763);
            ctx.qcurve_to(23.899494171142578,9.899497032165527,24.748737335205078,24.748737335205078);
            ctx.fill();
            break;
        case PetalID::kYinYang:
            ctx.set_line_width(3);
            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xffcfcfcf);
            ctx.begin_path();
            ctx.partial_arc(0,0,r,M_PI/2,3*M_PI/2,0);
            ctx.partial_arc(0,-r/2,r/2,-M_PI/2,M_PI/2,0);
            ctx.partial_arc(0,r/2,r/2,-M_PI/2,M_PI/2,1);
            ctx.fill();
            ctx.stroke();
            ctx.set_fill(0xff333333);
            ctx.set_stroke(0xff292929);
            ctx.begin_path();
            ctx.partial_arc(0,0,r,-M_PI/2,M_PI/2,0);
            ctx.partial_arc(0,r/2,r/2,M_PI/2,3*M_PI/2,0);
            ctx.partial_arc(0,-r/2,r/2,M_PI/2,3*M_PI/2,1);
            ctx.fill();
            ctx.stroke();
            ctx.set_stroke(0xffcfcfcf);
            ctx.begin_path();
            ctx.partial_arc(0,0,r,M_PI,3*M_PI/2,0);
            ctx.partial_arc(0,-r/2,r/2,-M_PI/2,M_PI/2,0);
            ctx.stroke();
            break;
        case PetalID::kYggdrasil:
            ctx.scale(r / 255);
            ctx.set_fill(0xff886d35);
            ctx.begin_path();
            ctx.move_to(-273.54, -218.49);
            ctx.qcurve_to(-284.88, -187.49, -267.08, -151.41);
            ctx.qcurve_to(-262.72, -142.57, -254.82, -136.69);
            ctx.qcurve_to(-246.93, -130.80, -237.22, -129.15);
            ctx.qcurve_to(-222.87, -126.71, -208.62, -122.97);
            ctx.qcurve_to(-212.49, -112.19, -216.13, -100.75);
            ctx.qcurve_to(-218.33, -96.85, -219.63, -92.56);
            ctx.qcurve_to(-220.93, -88.27, -221.28, -83.80);
            ctx.qcurve_to(-229.91, -54.12, -240.77, -7.55);
            ctx.qcurve_to(-244.62, 8.97, -235.66, 23.37);
            ctx.qcurve_to(-226.71, 37.78, -210.19, 41.64);
            ctx.line_to(-199.07, 44.23);
            ctx.line_to(-203.90, 73.81);
            ctx.qcurve_to(-203.91, 73.87, -203.91, 73.93);
            ctx.qcurve_to(-203.92, 73.98, -203.93, 74.04);
            ctx.qcurve_to(-205.20, 82.09, -203.29, 90.01);
            ctx.qcurve_to(-201.38, 97.93, -196.58, 104.52);
            ctx.qcurve_to(-191.79, 111.10, -184.84, 115.35);
            ctx.qcurve_to(-177.88, 119.60, -169.84, 120.87);
            ctx.line_to(-168.30, 121.11);
            ctx.line_to(-169.08, 125.06);
            ctx.qcurve_to(-169.09, 125.10, -169.10, 125.14);
            ctx.qcurve_to(-169.11, 125.18, -169.11, 125.22);
            ctx.qcurve_to(-170.67, 133.22, -169.04, 141.20);
            ctx.qcurve_to(-167.42, 149.18, -162.86, 155.94);
            ctx.qcurve_to(-158.30, 162.69, -151.51, 167.19);
            ctx.qcurve_to(-144.72, 171.69, -136.72, 173.24);
            ctx.line_to(-118.37, 176.80);
            ctx.qcurve_to(-118.31, 176.82, -118.25, 176.83);
            ctx.qcurve_to(-118.18, 176.84, -118.12, 176.85);
            ctx.qcurve_to(-117.31, 178.64, -116.33, 180.35);
            ctx.qcurve_to(-115.35, 182.05, -114.21, 183.65);
            ctx.qcurve_to(-109.49, 190.30, -102.59, 194.63);
            ctx.qcurve_to(-95.69, 198.96, -87.66, 200.32);
            ctx.line_to(-69.22, 203.45);
            ctx.qcurve_to(-68.74, 203.53, -68.27, 203.60);
            ctx.qcurve_to(-67.79, 203.67, -67.31, 203.73);
            ctx.qcurve_to(-63.19, 213.00, -55.22, 219.27);
            ctx.qcurve_to(-47.25, 225.55, -37.27, 227.38);
            ctx.line_to(-18.90, 230.76);
            ctx.qcurve_to(-14.56, 231.55, -10.15, 231.41);
            ctx.qcurve_to(-5.73, 231.26, -1.46, 230.18);
            ctx.qcurve_to(2.41, 233.19, 6.88, 235.20);
            ctx.qcurve_to(11.34, 237.21, 16.16, 238.11);
            ctx.line_to(34.53, 241.54);
            ctx.qcurve_to(34.57, 241.55, 34.60, 241.56);
            ctx.qcurve_to(34.64, 241.56, 34.68, 241.57);
            ctx.qcurve_to(37.13, 242.02, 39.62, 242.17);
            ctx.qcurve_to(42.11, 242.32, 44.60, 242.16);
            ctx.qcurve_to(50.12, 249.01, 57.96, 252.99);
            ctx.qcurve_to(65.80, 256.97, 74.58, 257.39);
            ctx.line_to(93.24, 258.26);
            ctx.qcurve_to(94.95, 258.34, 96.66, 258.28);
            ctx.qcurve_to(98.37, 258.21, 100.07, 258.01);
            ctx.qcurve_to(105.67, 262.39, 112.42, 264.63);
            ctx.qcurve_to(119.17, 266.87, 126.27, 266.70);
            ctx.line_to(144.96, 266.26);
            ctx.line_to(144.99, 266.26);
            ctx.qcurve_to(146.79, 266.22, 148.59, 266.01);
            ctx.qcurve_to(150.38, 265.81, 152.15, 265.45);
            ctx.qcurve_to(158.42, 269.45, 165.70, 270.98);
            ctx.qcurve_to(172.97, 272.52, 180.32, 271.40);
            ctx.line_to(198.79, 268.59);
            ctx.line_to(198.83, 268.58);
            ctx.qcurve_to(215.60, 266.01, 225.64, 252.34);
            ctx.qcurve_to(235.68, 238.66, 233.11, 221.89);
            ctx.qcurve_to(232.88, 220.35, 232.63, 218.81);
            ctx.line_to(248.18, 211.12);
            ctx.line_to(248.21, 211.11);
            ctx.qcurve_to(263.41, 203.59, 268.84, 187.51);
            ctx.qcurve_to(274.27, 171.44, 266.74, 156.23);
            ctx.qcurve_to(262.68, 148.04, 257.67, 140.39);
            ctx.qcurve_to(258.08, 139.88, 258.48, 139.35);
            ctx.qcurve_to(258.87, 138.83, 259.25, 138.29);
            ctx.line_to(269.93, 122.97);
            ctx.qcurve_to(277.99, 111.43, 277.25, 97.36);
            ctx.qcurve_to(276.50, 83.30, 267.27, 72.67);
            ctx.qcurve_to(267.91, 71.51, 268.47, 70.30);
            ctx.qcurve_to(269.04, 69.10, 269.52, 67.86);
            ctx.line_to(276.33, 50.46);
            ctx.line_to(276.35, 50.40);
            ctx.qcurve_to(279.31, 42.81, 279.14, 34.67);
            ctx.qcurve_to(278.97, 26.52, 275.69, 19.06);
            ctx.qcurve_to(272.55, 11.90, 266.98, 6.40);
            ctx.qcurve_to(261.41, 0.89, 254.21, -2.17);
            ctx.qcurve_to(254.51, -3.41, 254.73, -4.66);
            ctx.qcurve_to(254.96, -5.92, 255.10, -7.18);
            ctx.line_to(257.23, -25.75);
            ctx.qcurve_to(258.22, -34.46, 255.57, -42.82);
            ctx.qcurve_to(252.91, -51.18, 247.06, -57.71);
            ctx.qcurve_to(247.37, -59.47, 247.53, -61.25);
            ctx.qcurve_to(247.68, -63.03, 247.68, -64.81);
            ctx.line_to(247.68, -83.54);
            ctx.qcurve_to(247.67, -96.77, 239.92, -107.49);
            ctx.qcurve_to(232.17, -118.21, 219.62, -122.38);
            ctx.line_to(219.52, -129.09);
            ctx.qcurve_to(219.26, -146.05, 207.09, -157.86);
            ctx.qcurve_to(194.92, -169.68, 177.96, -169.43);
            ctx.qcurve_to(172.94, -169.35, 167.85, -169.16);
            ctx.line_to(167.84, -169.31);
            ctx.line_to(167.84, -169.34);
            ctx.qcurve_to(167.26, -186.29, 154.87, -197.87);
            ctx.qcurve_to(142.47, -209.45, 125.51, -208.88);
            ctx.qcurve_to(117.74, -208.61, 109.85, -208.00);
            ctx.qcurve_to(106.05, -222.95, 93.34, -231.68);
            ctx.qcurve_to(80.63, -240.40, 65.31, -238.58);
            ctx.qcurve_to(45.33, -236.20, 25.95, -232.73);
            ctx.qcurve_to(22.07, -247.33, 9.63, -255.91);
            ctx.qcurve_to(-2.81, -264.48, -17.83, -262.93);
            ctx.qcurve_to(-44.96, -260.13, -75.59, -250.78);
            ctx.qcurve_to(-79.88, -250.32, -83.97, -248.97);
            ctx.qcurve_to(-88.07, -247.62, -91.80, -245.44);
            ctx.qcurve_to(-103.36, -241.40, -116.91, -235.99);
            ctx.qcurve_to(-150.38, -254.09, -185.96, -269.59);
            ctx.qcurve_to(-192.91, -272.62, -200.49, -272.95);
            ctx.qcurve_to(-208.07, -273.29, -215.27, -270.89);
            ctx.qcurve_to(-216.07, -270.62, -216.85, -270.33);
            ctx.qcurve_to(-217.64, -270.03, -218.41, -269.70);
            ctx.qcurve_to(-261.32, -251.90, -273.54, -218.49);
            ctx.fill();
            ctx.set_fill(0xffa88642);
            ctx.begin_path();
            ctx.move_to(-230.34, -169.53);
            ctx.bcurve_to(-242.75, -194.66, -239.90, -216.60, -202.31, -232.03);
            ctx.line_to(-202.31, -232.03);
            ctx.bcurve_to(-175.35, -220.29, -147.49, -206.55, -119.56, -190.60);
            ctx.bcurve_to(-103.63, -197.35, -87.58, -203.79, -71.38, -209.13);
            ctx.line_to(-71.16, -210.06);
            ctx.bcurve_to(-70.64, -209.95, -70.14, -209.84, -69.63, -209.72);
            ctx.bcurve_to(-51.16, -215.71, -32.51, -220.24, -13.63, -222.19);
            ctx.line_to(-11.69, -203.60);
            ctx.bcurve_to(-18.65, -202.88, -25.65, -201.75, -32.69, -200.28);
            ctx.bcurve_to(-18.86, -196.28, -5.65, -191.88, 6.97, -187.13);
            ctx.bcurve_to(28.05, -191.87, 49.12, -195.40, 70.16, -197.91);
            ctx.line_to(72.38, -179.35);
            ctx.bcurve_to(60.85, -177.97, 49.33, -176.28, 37.81, -174.25);
            ctx.bcurve_to(47.45, -169.82, 56.68, -165.19, 65.50, -160.31);
            ctx.bcurve_to(85.48, -164.46, 105.90, -167.22, 126.91, -167.94);
            ctx.line_to(127.53, -149.28);
            ctx.bcurve_to(114.77, -148.85, 102.19, -147.59, 89.72, -145.69);
            ctx.bcurve_to(100.48, -138.62, 110.54, -131.19, 119.88, -123.44);
            ctx.bcurve_to(139.03, -126.34, 158.55, -128.18, 178.56, -128.47);
            ctx.line_to(178.85, -109.78);
            ctx.bcurve_to(165.10, -109.58, 151.54, -108.61, 138.13, -107.03);
            ctx.bcurve_to(146.65, -98.70, 154.45, -90.07, 161.53, -81.16);
            ctx.bcurve_to(176.57, -82.67, 191.64, -83.48, 206.72, -83.50);
            ctx.line_to(206.72, -64.81);
            ctx.bcurve_to(195.94, -64.80, 185.15, -64.36, 174.35, -63.53);
            ctx.bcurve_to(178.70, -57.00, 182.72, -50.36, 186.38, -43.59);
            ctx.bcurve_to(188.32, -39.99, 190.16, -36.35, 191.91, -32.68);
            ctx.bcurve_to(200.12, -32.12, 208.33, -31.35, 216.53, -30.40);
            ctx.line_to(214.41, -11.84);
            ctx.bcurve_to(209.66, -12.39, 204.90, -12.86, 200.16, -13.28);
            ctx.bcurve_to(204.99, -0.35, 208.65, 12.87, 211.19, 26.25);
            ctx.bcurve_to(220.15, 28.94, 229.14, 32.00, 238.19, 35.53);
            ctx.line_to(231.38, 52.94);
            ctx.bcurve_to(225.61, 50.69, 219.89, 48.60, 214.19, 46.72);
            ctx.bcurve_to(215.56, 59.97, 215.87, 73.34, 215.16, 86.75);
            ctx.bcurve_to(222.29, 90.45, 229.37, 94.66, 236.35, 99.53);
            ctx.line_to(225.66, 114.84);
            ctx.bcurve_to(221.59, 112.00, 217.47, 109.40, 213.31, 107.00);
            ctx.bcurve_to(211.78, 118.85, 209.49, 130.70, 206.41, 142.47);
            ctx.bcurve_to(215.97, 151.68, 224.08, 162.38, 230.03, 174.41);
            ctx.line_to(213.28, 182.69);
            ctx.bcurve_to(209.82, 175.69, 205.38, 169.15, 200.19, 163.16);
            ctx.bcurve_to(196.27, 174.68, 191.60, 186.11, 186.25, 197.37);
            ctx.bcurve_to(188.93, 207.61, 191.06, 217.85, 192.63, 228.09);
            ctx.line_to(174.16, 230.91);
            ctx.bcurve_to(172.83, 222.25, 171.08, 213.58, 168.91, 204.91);
            ctx.bcurve_to(160.54, 204.65, 152.23, 204.20, 144.00, 203.56);
            ctx.bcurve_to(143.81, 210.75, 143.82, 218.00, 144.00, 225.31);
            ctx.line_to(125.31, 225.75);
            ctx.bcurve_to(125.12, 217.76, 125.12, 209.77, 125.34, 201.81);
            ctx.bcurve_to(115.61, 200.71, 106.02, 199.31, 96.53, 197.66);
            ctx.bcurve_to(95.94, 204.19, 95.46, 210.76, 95.16, 217.34);
            ctx.line_to(76.50, 216.47);
            ctx.bcurve_to(76.85, 208.96, 77.40, 201.51, 78.09, 194.10);
            ctx.bcurve_to(66.82, 191.67, 55.75, 188.82, 44.94, 185.60);
            ctx.bcurve_to(43.99, 190.82, 43.04, 196.05, 42.06, 201.28);
            ctx.line_to(23.69, 197.84);
            ctx.bcurve_to(24.80, 191.86, 25.88, 185.85, 26.97, 179.84);
            ctx.bcurve_to(15.32, 175.79, 3.99, 171.23, -6.97, 166.22);
            ctx.bcurve_to(-8.51, 174.30, -10.01, 182.39, -11.50, 190.47);
            ctx.line_to(-29.88, 187.10);
            ctx.bcurve_to(-28.07, 177.30, -26.25, 167.49, -24.38, 157.69);
            ctx.bcurve_to(-36.05, 151.58, -47.24, 144.90, -57.91, 137.63);
            ctx.bcurve_to(-59.44, 146.09, -60.93, 154.57, -62.38, 163.06);
            ctx.line_to(-80.81, 159.94);
            ctx.bcurve_to(-78.85, 148.41, -76.80, 136.89, -74.66, 125.38);
            ctx.bcurve_to(-84.83, 117.40, -94.38, 108.82, -103.31, 99.66);
            ctx.bcurve_to(-105.78, 111.96, -108.18, 124.28, -110.56, 136.59);
            ctx.line_to(-128.91, 133.03);
            ctx.bcurve_to(-125.64, 116.14, -122.30, 99.24, -118.84, 82.31);
            ctx.bcurve_to(-126.58, 72.92, -133.71, 63.02, -140.13, 52.59);
            ctx.bcurve_to(-141.79, 62.81, -143.39, 73.06, -145.00, 83.31);
            ctx.line_to(-163.47, 80.41);
            ctx.bcurve_to(-160.61, 62.22, -157.70, 43.99, -154.56, 25.78);
            ctx.bcurve_to(-162.84, 8.07, -169.33, -10.85, -173.78, -30.97);
            ctx.bcurve_to(-176.87, -18.73, -179.80, -6.39, -182.69, 6.00);
            ctx.line_to(-200.88, 1.75);
            ctx.bcurve_to(-194.65, -24.93, -188.20, -51.67, -180.28, -78.00);
            ctx.bcurve_to(-180.34, -78.89, -180.39, -79.77, -180.44, -80.66);
            ctx.line_to(-179.47, -80.72);
            ctx.bcurve_to(-172.47, -103.66, -164.33, -126.26, -154.19, -148.19);
            ctx.bcurve_to(-179.16, -158.00, -204.57, -165.14, -230.34, -169.53);
            ctx.move_to(-25.91, -178.72);
            ctx.bcurve_to(-39.40, -182.96, -53.58, -186.83, -68.44, -190.28);
            ctx.line_to(-68.44, -190.28);
            ctx.bcurve_to(-78.68, -186.81, -89.00, -182.86, -99.38, -178.66);
            ctx.bcurve_to(-90.90, -173.48, -82.42, -168.10, -74.00, -162.50);
            ctx.bcurve_to(-57.97, -168.72, -41.93, -174.11, -25.91, -178.72);
            ctx.move_to(5.64, -167.66);
            ctx.bcurve_to(5.64, -167.66, 5.63, -167.65, 5.63, -167.66);
            ctx.line_to(5.63, -167.66);
            ctx.bcurve_to(-14.67, -162.88, -34.94, -156.89, -55.22, -149.56);
            ctx.bcurve_to(-46.91, -143.66, -38.64, -137.56, -30.47, -131.22);
            ctx.bcurve_to(-8.04, -139.55, 14.63, -147.37, 37.84, -153.66);
            ctx.bcurve_to(27.65, -158.60, 16.92, -163.27, 5.66, -167.66);
            ctx.bcurve_to(5.65, -167.66, 5.65, -167.66, 5.64, -167.66);
            ctx.move_to(-136.94, -140.94);
            ctx.bcurve_to(-146.66, -119.98, -154.57, -98.17, -161.41, -75.81);
            ctx.bcurve_to(-159.70, -51.76, -155.24, -29.33, -148.31, -8.53);
            ctx.bcurve_to(-140.32, -49.57, -130.31, -90.46, -115.69, -130.75);
            ctx.bcurve_to(-122.72, -134.37, -129.82, -137.76, -136.94, -140.94);
            ctx.move_to(95.19, -119.13);
            ctx.bcurve_to(85.12, -126.62, 74.26, -133.80, 62.56, -140.56);
            ctx.bcurve_to(36.94, -134.78, 11.74, -126.62, -13.40, -117.53);
            ctx.bcurve_to(-5.49, -110.98, 2.33, -104.22, 10.03, -97.22);
            ctx.bcurve_to(37.95, -105.53, 66.19, -113.36, 95.19, -119.13);
            ctx.move_to(-71.44, -104.72);
            ctx.bcurve_to(-80.56, -110.77, -89.76, -116.46, -99.06, -121.75);
            ctx.bcurve_to(-115.97, -74.44, -126.56, -25.84, -135.19, 23.38);
            ctx.bcurve_to(-129.04, 35.76, -121.89, 47.43, -113.87, 58.44);
            ctx.bcurve_to(-102.36, 4.19, -89.13, -50.22, -71.44, -104.72);
            ctx.move_to(139.44, -78.47);
            ctx.bcurve_to(131.88, -87.13, 123.53, -95.53, 114.38, -103.59);
            ctx.line_to(114.38, -103.59);
            ctx.bcurve_to(84.47, -98.53, 55.16, -90.83, 25.88, -82.31);
            ctx.bcurve_to(33.85, -74.56, 41.70, -66.58, 49.34, -58.31);
            ctx.bcurve_to(79.28, -67.12, 109.31, -74.12, 139.44, -78.47);
            ctx.move_to(-22.37, -67.75);
            ctx.bcurve_to(-33.21, -76.95, -44.20, -85.58, -55.34, -93.62);
            ctx.bcurve_to(-73.50, -36.79, -86.93, 20.16, -98.75, 77.28);
            ctx.bcurve_to(-90.13, 87.00, -80.74, 96.09, -70.65, 104.56);
            ctx.bcurve_to(-59.15, 46.57, -44.45, -11.13, -22.37, -67.75);
            ctx.move_to(169.94, -34.69);
            ctx.bcurve_to(165.02, -43.79, 159.40, -52.71, 153.06, -61.41);
            ctx.bcurve_to(123.19, -57.76, 93.22, -51.30, 63.16, -42.84);
            ctx.bcurve_to(67.65, -37.64, 72.07, -32.34, 76.41, -26.94);
            ctx.bcurve_to(101.98, -31.44, 127.56, -33.67, 153.09, -33.88);
            ctx.bcurve_to(158.89, -33.92, 164.68, -33.84, 170.47, -33.69);
            ctx.bcurve_to(170.29, -34.02, 170.12, -34.35, 169.94, -34.69);
            ctx.move_to(21.07, -26.87);
            ctx.bcurve_to(11.71, -36.57, 2.23, -45.82, -7.41, -54.59);
            ctx.bcurve_to(-28.73, 1.60, -42.90, 59.16, -54.13, 117.44);
            ctx.bcurve_to(-43.57, 125.07, -32.39, 132.13, -20.63, 138.53);
            ctx.bcurve_to(-9.69, 83.54, 3.17, 28.40, 21.07, -26.87);
            ctx.move_to(179.50, -14.69);
            ctx.bcurve_to(171.49, -15.06, 163.47, -15.24, 155.47, -15.22);
            ctx.bcurve_to(133.43, -15.15, 111.42, -13.49, 89.41, -10.13);
            ctx.bcurve_to(94.86, -2.81, 100.19, 4.65, 105.34, 12.31);
            ctx.bcurve_to(107.42, 12.27, 109.50, 12.23, 111.56, 12.22);
            ctx.bcurve_to(138.67, 12.05, 164.85, 14.76, 191.00, 20.84);
            ctx.bcurve_to(188.19, 8.79, 184.37, -3.08, 179.50, -14.69);
            ctx.move_to(66.34, 24.97);
            ctx.bcurve_to(56.34, 12.32, 46.12, 0.28, 35.72, -11.22);
            ctx.line_to(35.72, -11.22);
            ctx.bcurve_to(19.22, 41.49, 7.12, 94.33, -3.31, 147.31);
            ctx.bcurve_to(7.52, 152.42, 18.76, 157.03, 30.37, 161.16);
            ctx.bcurve_to(38.68, 115.57, 47.86, 69.58, 66.34, 24.97);
            ctx.move_to(194.75, 41.00);
            ctx.bcurve_to(169.21, 34.32, 143.84, 31.17, 117.31, 30.94);
            ctx.line_to(117.31, 30.94);
            ctx.bcurve_to(123.40, 40.80, 129.23, 50.92, 134.78, 61.34);
            ctx.bcurve_to(155.37, 65.37, 176.27, 70.11, 196.84, 78.34);
            ctx.bcurve_to(197.13, 65.80, 196.46, 53.32, 194.75, 41.00);
            ctx.move_to(101.44, 73.03);
            ctx.bcurve_to(94.28, 62.46, 87.00, 52.21, 79.59, 42.28);
            ctx.bcurve_to(64.33, 82.45, 56.09, 124.45, 48.31, 167.06);
            ctx.bcurve_to(58.69, 170.21, 69.32, 172.96, 80.19, 175.34);
            ctx.bcurve_to(84.74, 140.42, 92.56, 106.48, 101.44, 73.03);
            ctx.move_to(195.59, 98.09);
            ctx.bcurve_to(179.43, 91.04, 162.71, 86.42, 145.56, 82.66);
            ctx.line_to(145.56, 82.66);
            ctx.bcurve_to(150.27, 92.50, 154.68, 102.61, 158.87, 112.94);
            ctx.bcurve_to(169.94, 117.04, 180.61, 122.52, 190.37, 129.31);
            ctx.bcurve_to(192.76, 118.91, 194.51, 108.49, 195.59, 98.09);
            ctx.move_to(136.28, 128.75);
            ctx.bcurve_to(129.43, 116.88, 122.41, 105.34, 115.28, 94.13);
            ctx.bcurve_to(108.22, 122.18, 102.23, 150.35, 98.56, 178.97);
            ctx.bcurve_to(107.69, 180.58, 116.96, 181.94, 126.34, 183.03);
            ctx.bcurve_to(127.83, 164.70, 130.88, 146.55, 136.28, 128.75);
            ctx.move_to(185.13, 148.81);
            ctx.bcurve_to(179.87, 144.69, 174.22, 141.02, 168.31, 137.81);
            ctx.bcurve_to(171.85, 147.90, 175.14, 158.22, 178.16, 168.75);
            ctx.bcurve_to(180.73, 162.13, 183.07, 155.48, 185.13, 148.81);
            ctx.move_to(154.56, 161.91);
            ctx.bcurve_to(152.92, 158.79, 151.26, 155.70, 149.59, 152.63);
            ctx.bcurve_to(147.39, 163.18, 145.89, 173.92, 145.00, 184.84);
            ctx.bcurve_to(151.12, 185.32, 157.26, 185.71, 163.47, 185.97);
            ctx.bcurve_to(160.88, 177.96, 157.91, 169.93, 154.56, 161.90);
            ctx.fill();
            break;
        case PetalID::kRice:
            ctx.set_stroke(0xffcfcfcf);
            ctx.set_line_width(9);
            ctx.scale(r / 13);
            ctx.begin_path();
            ctx.move_to(-8,0);
            ctx.qcurve_to(0,-3.5,8,0);
            ctx.stroke();
            ctx.set_stroke(0xffffffff);
            ctx.set_line_width(5);
            ctx.stroke();
            break;
        case PetalID::kBone:
            ctx.set_fill(0xffffffff);
            ctx.set_stroke(0xffcfcfcf);
            ctx.set_line_width(5);
            ctx.scale(r / 12);
            ctx.begin_path();
            ctx.move_to(-10,-4);
            ctx.qcurve_to(0,0,10,-4);
            ctx.bcurve_to(14,-10,20,-2,14,0);
            ctx.bcurve_to(20,2,14,10,10,4);
            ctx.qcurve_to(0,0,-10,4);
            ctx.bcurve_to(-14,10,-20,2,-14,0);
            ctx.bcurve_to(-20,-2,-14,-10,-10,-4);
            ctx.stroke();
            ctx.fill();
            break;
        case PetalID::kYucca:
            ctx.set_fill(0xff74b53f);
            ctx.set_stroke(0xff5e9333);
            ctx.set_line_width(3);
            ctx.begin_path();
            ctx.move_to(14,0);
            ctx.qcurve_to(0,-12,-14,0);
            ctx.qcurve_to(0,12,14,0);
            ctx.fill();
            ctx.stroke();
            ctx.set_line_width(2);
            ctx.begin_path();
            ctx.move_to(14,0);
            ctx.qcurve_to(0,-3,-14,0);
            ctx.stroke();
            break;
        case PetalID::kCorn:
            ctx.scale(r / 10);
            ctx.set_fill(0xffffe419);
            ctx.set_stroke(0xffcfb914);
            ctx.set_line_width(2);
            ctx.begin_path();
            ctx.move_to(-5,8);
            ctx.qcurve_to(-15,-8,0,-8);
            ctx.qcurve_to(15,-8,5,8);
            ctx.qcurve_to(0,2,-5,8);
            ctx.fill();
            ctx.stroke();
            break;
        case PetalID::kCommonRoot:
        case PetalID::kUnusualRoot:
        case PetalID::kEpicRoot:
        case PetalID::kLegendaryRoot:
        case PetalID::kMythicRoot:
        case PetalID::kUniqueRoot:
        case PetalID::kRoot:
            ctx.scale(r / 7);
            ctx.set_fill(0xffb86c32);
            ctx.begin_path();
            ctx.move_to(-0.805, 9.552);
            ctx.qcurve_to(-2.432, 4.127, -4.059, -1.298);
            ctx.qcurve_to(-2.585, -4.731, -1.110, -8.164);
            ctx.qcurve_to(-2.846, -10.009, -4.582, -11.853);
            ctx.qcurve_to(-4.435, -13.219, -4.288, -14.585);
            ctx.qcurve_to(-3.586, -13.404, -2.884, -12.223);
            ctx.qcurve_to(0.032, -11.081, 2.949, -9.938);
            ctx.qcurve_to(2.176, -5.324, 1.404, -0.710);
            ctx.qcurve_to(4.875, 2.979, 8.346, 6.668);
            ctx.qcurve_to(5.966, 6.965, 3.585, 7.262);
            ctx.qcurve_to(1.390, 8.407, -0.805, 9.552);
            ctx.fill();

            ctx.set_fill(0xff955728);
            ctx.begin_path();
            ctx.move_to(-1.688, 9.817);
            ctx.qcurve_to(-3.315, 4.392, -4.942, -1.033);
            ctx.qcurve_to(-4.988, -1.188, -4.979, -1.350);
            ctx.qcurve_to(-4.970, -1.512, -4.906, -1.661);
            ctx.qcurve_to(-3.431, -5.095, -1.956, -8.528);
            ctx.qcurve_to(-1.533, -8.346, -1.110, -8.164);
            ctx.qcurve_to(-1.445, -7.849, -1.781, -7.533);
            ctx.qcurve_to(-3.517, -9.377, -5.252, -11.222);
            ctx.qcurve_to(-5.321, -11.294, -5.372, -11.380);
            ctx.qcurve_to(-5.423, -11.466, -5.455, -11.560);
            ctx.qcurve_to(-5.487, -11.655, -5.498, -11.754);
            ctx.qcurve_to(-5.508, -11.853, -5.498, -11.951);
            ctx.qcurve_to(-5.351, -13.317, -5.204, -14.683);
            ctx.qcurve_to(-5.199, -14.728, -5.190, -14.773);
            ctx.qcurve_to(-5.181, -14.817, -5.167, -14.860);
            ctx.qcurve_to(-5.154, -14.903, -5.136, -14.945);
            ctx.qcurve_to(-5.118, -14.987, -5.096, -15.026);
            ctx.qcurve_to(-5.075, -15.066, -5.049, -15.104);
            ctx.qcurve_to(-5.023, -15.142, -4.994, -15.176);
            ctx.qcurve_to(-4.966, -15.210, -4.933, -15.242);
            ctx.qcurve_to(-4.901, -15.275, -4.866, -15.302);
            ctx.qcurve_to(-4.831, -15.330, -4.793, -15.355);
            ctx.qcurve_to(-4.755, -15.381, -4.715, -15.401);
            ctx.qcurve_to(-4.674, -15.422, -4.633, -15.439);
            ctx.qcurve_to(-4.592, -15.456, -4.547, -15.469);
            ctx.qcurve_to(-4.502, -15.481, -4.459, -15.490);
            ctx.qcurve_to(-4.416, -15.499, -4.370, -15.502);
            ctx.qcurve_to(-4.323, -15.505, -4.279, -15.506);
            ctx.qcurve_to(-4.235, -15.507, -4.189, -15.501);
            ctx.qcurve_to(-4.082, -15.489, -3.981, -15.453);
            ctx.qcurve_to(-3.880, -15.418, -3.790, -15.360);
            ctx.qcurve_to(-3.699, -15.302, -3.625, -15.225);
            ctx.qcurve_to(-3.551, -15.148, -3.496, -15.056);
            ctx.qcurve_to(-2.794, -13.875, -2.092, -12.694);
            ctx.qcurve_to(-2.488, -12.459, -2.884, -12.223);
            ctx.qcurve_to(-2.716, -12.652, -2.548, -13.081);
            ctx.qcurve_to(0.369, -11.939, 3.285, -10.796);
            ctx.qcurve_to(3.358, -10.767, 3.426, -10.727);
            ctx.qcurve_to(3.493, -10.686, 3.553, -10.634);
            ctx.qcurve_to(3.612, -10.583, 3.662, -10.522);
            ctx.qcurve_to(3.711, -10.461, 3.750, -10.393);
            ctx.qcurve_to(3.789, -10.324, 3.816, -10.250);
            ctx.qcurve_to(3.842, -10.176, 3.856, -10.099);
            ctx.qcurve_to(3.870, -10.021, 3.870, -9.942);
            ctx.qcurve_to(3.871, -9.863, 3.858, -9.786);
            ctx.qcurve_to(3.085, -5.172, 2.313, -0.558);
            ctx.qcurve_to(1.858, -0.634, 1.404, -0.710);
            ctx.qcurve_to(1.739, -1.026, 2.075, -1.342);
            ctx.qcurve_to(5.546, 2.347, 9.018, 6.036);
            ctx.qcurve_to(9.049, 6.069, 9.076, 6.105);
            ctx.qcurve_to(9.104, 6.141, 9.128, 6.180);
            ctx.qcurve_to(9.152, 6.218, 9.172, 6.259);
            ctx.qcurve_to(9.193, 6.299, 9.208, 6.341);
            ctx.qcurve_to(9.224, 6.383, 9.236, 6.427);
            ctx.qcurve_to(9.248, 6.471, 9.255, 6.516);
            ctx.qcurve_to(9.263, 6.561, 9.266, 6.606);
            ctx.qcurve_to(9.269, 6.651, 9.268, 6.696);
            ctx.qcurve_to(9.267, 6.741, 9.260, 6.786);
            ctx.qcurve_to(9.254, 6.831, 9.245, 6.875);
            ctx.qcurve_to(9.235, 6.919, 9.220, 6.962);
            ctx.qcurve_to(9.205, 7.005, 9.187, 7.046);
            ctx.qcurve_to(9.168, 7.088, 9.146, 7.127);
            ctx.qcurve_to(9.123, 7.166, 9.097, 7.203);
            ctx.qcurve_to(9.070, 7.240, 9.041, 7.274);
            ctx.qcurve_to(9.011, 7.308, 8.978, 7.339);
            ctx.qcurve_to(8.871, 7.439, 8.739, 7.502);
            ctx.qcurve_to(8.606, 7.564, 8.461, 7.582);
            ctx.qcurve_to(6.080, 7.879, 3.700, 8.176);
            ctx.qcurve_to(3.643, 7.718, 3.586, 7.261);
            ctx.qcurve_to(3.799, 7.669, 4.012, 8.078);
            ctx.qcurve_to(1.817, 9.223, -0.379, 10.369);
            ctx.qcurve_to(-0.419, 10.390, -0.461, 10.406);
            ctx.qcurve_to(-0.503, 10.424, -0.547, 10.437);
            ctx.qcurve_to(-0.590, 10.449, -0.634, 10.457);
            ctx.qcurve_to(-0.679, 10.466, -0.724, 10.470);
            ctx.qcurve_to(-0.769, 10.474, -0.814, 10.473);
            ctx.qcurve_to(-0.860, 10.473, -0.905, 10.468);
            ctx.qcurve_to(-0.949, 10.463, -0.994, 10.454);
            ctx.qcurve_to(-1.038, 10.444, -1.081, 10.431);
            ctx.qcurve_to(-1.124, 10.417, -1.166, 10.400);
            ctx.qcurve_to(-1.208, 10.382, -1.248, 10.360);
            ctx.qcurve_to(-1.287, 10.338, -1.325, 10.313);
            ctx.qcurve_to(-1.362, 10.287, -1.397, 10.258);
            ctx.qcurve_to(-1.431, 10.229, -1.463, 10.197);
            ctx.qcurve_to(-1.496, 10.164, -1.523, 10.129);
            ctx.qcurve_to(-1.551, 10.094, -1.576, 10.056);
            ctx.qcurve_to(-1.601, 10.018, -1.622, 9.978);
            ctx.qcurve_to(-1.662, 9.901, -1.688, 9.816);
            ctx.fill();
            ctx.begin_path();
            ctx.move_to(0.077, 9.287);
            ctx.qcurve_to(-0.364, 9.419, -0.805, 9.552);
            ctx.qcurve_to(-1.018, 9.143, -1.232, 8.735);
            ctx.qcurve_to(0.964, 7.589, 3.160, 6.444);
            ctx.qcurve_to(3.307, 6.367, 3.472, 6.346);
            ctx.qcurve_to(5.852, 6.050, 8.233, 5.753);
            ctx.qcurve_to(8.290, 6.210, 8.347, 6.668);
            ctx.qcurve_to(8.011, 6.983, 7.676, 7.299);
            ctx.qcurve_to(4.204, 3.610, 0.733, -0.079);
            ctx.qcurve_to(0.660, -0.157, 0.606, -0.249);
            ctx.qcurve_to(0.553, -0.341, 0.522, -0.443);
            ctx.qcurve_to(0.491, -0.545, 0.484, -0.651);
            ctx.qcurve_to(0.477, -0.758, 0.495, -0.863);
            ctx.qcurve_to(1.268, -5.476, 2.040, -10.090);
            ctx.qcurve_to(2.495, -10.014, 2.949, -9.939);
            ctx.qcurve_to(2.781, -9.510, 2.613, -9.081);
            ctx.qcurve_to(-0.304, -10.223, -3.220, -11.366);
            ctx.qcurve_to(-3.363, -11.422, -3.480, -11.521);
            ctx.qcurve_to(-3.597, -11.621, -3.676, -11.753);
            ctx.qcurve_to(-4.378, -12.934, -5.080, -14.114);
            ctx.qcurve_to(-4.684, -14.350, -4.288, -14.585);
            ctx.qcurve_to(-3.830, -14.536, -3.372, -14.486);
            ctx.qcurve_to(-3.518, -13.121, -3.665, -11.755);
            ctx.qcurve_to(-4.123, -11.805, -4.581, -11.854);
            ctx.qcurve_to(-4.246, -12.169, -3.910, -12.485);
            ctx.qcurve_to(-2.175, -10.641, -0.439, -8.796);
            ctx.qcurve_to(-0.393, -8.747, -0.355, -8.693);
            ctx.qcurve_to(-0.316, -8.637, -0.286, -8.578);
            ctx.qcurve_to(-0.256, -8.518, -0.235, -8.454);
            ctx.qcurve_to(-0.214, -8.391, -0.203, -8.325);
            ctx.qcurve_to(-0.191, -8.260, -0.189, -8.192);
            ctx.qcurve_to(-0.187, -8.124, -0.195, -8.059);
            ctx.qcurve_to(-0.202, -7.993, -0.220, -7.927);
            ctx.qcurve_to(-0.237, -7.862, -0.263, -7.801);
            ctx.qcurve_to(-1.738, -4.368, -3.213, -0.934);
            ctx.qcurve_to(-3.636, -1.116, -4.059, -1.298);
            ctx.qcurve_to(-3.618, -1.430, -3.176, -1.563);
            ctx.qcurve_to(-1.550, 3.862, 0.077, 9.287);
            ctx.fill();
            break;
        default: {
            // Wave-system rarity expansion: every new (base, rarity)
            // PetalID added at the end of the enum aliases its rendering
            // to the first earlier-indexed petal with the same name.
            // kLegendaryBasic → kBasic, kUniqueRose → kRose, etc. Avoids
            // having to add ~120 explicit fall-through case labels here.
            // PETAL_DATA is indexed in the same order as the enum, so a
            // forward scan from id=1 finds the canonical case.
            char const *name = PETAL_DATA[id].name;
            for (PetalID::T fallback = 1; fallback < id; ++fallback) {
                if (std::strcmp(PETAL_DATA[fallback].name, name) == 0) {
                    draw_static_petal_single(fallback, ctx);
                    return;
                }
            }
            assert(id < PetalID::kNumPetals);
            assert(!"didn't cover petal render");
            break;
        }
    }
}

void draw_static_petal(PetalID::T id, Renderer &ctx) {
    struct PetalData const &data = PETAL_DATA[id];
    uint32_t count = data.count;
    if (count == 0) count = 1;
    for (uint32_t i = 0; i < count; ++i) {
        RenderContext context(&ctx);
        float rad = 10;
        if (data.attributes.clump_radius != 0)
            rad = data.attributes.clump_radius;
        ctx.rotate(i * 2 * M_PI / data.count);
        if (data.count > 1) ctx.translate(rad, 0);
        ctx.rotate(data.attributes.icon_angle);
        draw_static_petal_single(id, ctx);
    }
}

void draw_loadout_background(Renderer &ctx, uint8_t id, float reload) {
    RenderContext c(&ctx);
    ctx.set_fill(Renderer::HSV(RARITY_COLORS[PETAL_DATA[id].rarity], 0.8));
    ctx.round_line_join();
    ctx.round_line_cap();
    ctx.begin_path();
    ctx.round_rect(-30, -30, 60, 60, 3);
    ctx.fill();
    ctx.set_fill(RARITY_COLORS[PETAL_DATA[id].rarity]);
    ctx.begin_path();
    ctx.rect(-25, -25, 50, 50);
    ctx.fill();
    ctx.clip();
    if (reload < 1) {
        float rld =  1 - (float) reload;
        {
            rld = rld * rld * rld * (rld * (6.0f * rld - 15.0f) + 10.0f);
            RenderContext context(&ctx);
            ctx.set_fill(0x40000000);
            ctx.begin_path();
            ctx.move_to(0,0);
            ctx.partial_arc(0, 0, 90, -M_PI / 2 - rld * M_PI * 10, -M_PI / 2 - rld * M_PI * 8, 0);
            ctx.fill();
        }
    }
    ctx.translate(0, -5);
    {
        RenderContext r(&ctx);
        ctx.scale(0.833);
        if (PETAL_DATA[id].radius > 20) ctx.scale(20 / PETAL_DATA[id].radius);
        draw_static_petal(id, ctx);
    }
    float text_width = 12 * Renderer::get_ascii_text_size(PETAL_DATA[id].name);
    if (text_width < 50) text_width = 12;
    else text_width = 12 * 50 / text_width;
    ctx.translate(0, 20);
    ctx.draw_text(PETAL_DATA[id].name, { .size = text_width });
}