#include <Server/TiledMap.hh>

#include <Server/Spawn.hh>

#include <Shared/Entity.hh>
#include <Shared/Helpers.hh>
#include <Shared/Map.hh>
#include <Shared/Simulation.hh>
#include <Shared/StaticData.hh>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <zlib.h>

namespace TiledMap {
    bool loaded = false;
    std::vector<TiledCollisionRect> collision_rects;
    std::vector<TiledCollisionPoly> collision_polys;
    std::vector<TiledSpawnPolygon> spawn_polygons;
    static std::vector<uint32_t> mob_counts;

    // Cache of per-tile polygons in image-local 0..1 coords, keyed by base
    // gid (flags stripped). Populated on demand from the SVG files in the
    // tileset; the per-cell collision_polys vector is built from these by
    // applying the cell's flip flags + grid translation at load time.
    static std::unordered_map<uint32_t, std::vector<TiledPolyVert>> tile_shape_cache;
}

// ---------------------------------------------------------------------------
// Minimal JSON parser, just enough for Tiled's JSON output. Supports objects,
// arrays, strings, numbers, booleans, null. No \uXXXX escapes — Tiled doesn't
// emit them for the fields we care about. Throws std::runtime_error on bad
// input; the loader catches and reports it.
// ---------------------------------------------------------------------------
namespace {

struct Json {
    enum Type { kNull, kBool, kNum, kStr, kArr, kObj };
    Type type = kNull;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    Json const *find(std::string const &k) const {
        if (type != kObj) return nullptr;
        for (auto const &kv : obj) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    double as_num(double def = 0) const { return type == kNum ? num : def; }
    std::string const &as_str() const { static std::string e; return type == kStr ? str : e; }
    bool as_bool(bool def = false) const { return type == kBool ? b : def; }
};

class Parser {
public:
    Parser(char const *p, char const *e) : p_(p), end_(e) {}
    Json parse() { skip(); Json v = value(); skip(); return v; }
private:
    char const *p_, *end_;

    void skip() { while (p_ < end_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\n' || *p_ == '\r')) ++p_; }
    void fail(char const *msg) { throw std::runtime_error(std::string("json: ") + msg); }

    Json value() {
        skip();
        if (p_ >= end_) fail("eof");
        char c = *p_;
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return string();
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        if (c == 't' || c == 'f') return boolean();
        if (c == 'n') return null_();
        fail("unexpected char");
        return Json{};
    }

    Json object() {
        Json out; out.type = Json::kObj;
        ++p_; skip();
        if (p_ < end_ && *p_ == '}') { ++p_; return out; }
        while (p_ < end_) {
            skip();
            if (*p_ != '"') fail("expected key");
            Json k = string();
            skip();
            if (p_ >= end_ || *p_ != ':') fail("expected ':'");
            ++p_;
            Json v = value();
            out.obj.push_back({k.str, std::move(v)});
            skip();
            if (p_ < end_ && *p_ == ',') { ++p_; continue; }
            if (p_ < end_ && *p_ == '}') { ++p_; return out; }
            fail("expected ',' or '}'");
        }
        fail("unterminated object");
        return out;
    }

    Json array() {
        Json out; out.type = Json::kArr;
        ++p_; skip();
        if (p_ < end_ && *p_ == ']') { ++p_; return out; }
        while (p_ < end_) {
            Json v = value();
            out.arr.push_back(std::move(v));
            skip();
            if (p_ < end_ && *p_ == ',') { ++p_; continue; }
            if (p_ < end_ && *p_ == ']') { ++p_; return out; }
            fail("expected ',' or ']'");
        }
        fail("unterminated array");
        return out;
    }

    Json string() {
        Json out; out.type = Json::kStr;
        ++p_;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                ++p_;
                if (p_ >= end_) fail("bad escape");
                char c = *p_++;
                switch (c) {
                    case '"': out.str += '"'; break;
                    case '\\': out.str += '\\'; break;
                    case '/': out.str += '/'; break;
                    case 'n': out.str += '\n'; break;
                    case 't': out.str += '\t'; break;
                    case 'r': out.str += '\r'; break;
                    case 'b': out.str += '\b'; break;
                    case 'f': out.str += '\f'; break;
                    case 'u': {
                        if (end_ - p_ < 4) fail("bad \\u");
                        p_ += 4;
                        out.str += '?';
                        break;
                    }
                    default: out.str += c; break;
                }
            } else {
                out.str += *p_++;
            }
        }
        if (p_ >= end_) fail("unterminated string");
        ++p_;
        return out;
    }

    Json number() {
        Json out; out.type = Json::kNum;
        char const *s = p_;
        if (*p_ == '-') ++p_;
        while (p_ < end_ && ((*p_ >= '0' && *p_ <= '9') || *p_ == '.' || *p_ == 'e' || *p_ == 'E' || *p_ == '+' || *p_ == '-')) ++p_;
        out.num = std::strtod(std::string(s, p_).c_str(), nullptr);
        return out;
    }

    Json boolean() {
        Json out; out.type = Json::kBool;
        if (end_ - p_ >= 4 && std::strncmp(p_, "true", 4) == 0) { p_ += 4; out.b = true; return out; }
        if (end_ - p_ >= 5 && std::strncmp(p_, "false", 5) == 0) { p_ += 5; out.b = false; return out; }
        fail("expected bool");
        return out;
    }

    Json null_() {
        Json out;
        if (end_ - p_ >= 4 && std::strncmp(p_, "null", 4) == 0) { p_ += 4; return out; }
        fail("expected null");
        return out;
    }
};

// -------- Name → MobID lookup (only known mobs; unknown names are dropped) --

MobID::T mob_id_from_name(std::string const &n) {
    static std::unordered_map<std::string, MobID::T> const map = {
        {"ant_baby", MobID::kBabyAnt},
        {"ant_worker", MobID::kWorkerAnt},
        {"ant_soldier", MobID::kSoldierAnt},
        {"ant_queen", MobID::kQueenAnt},
        {"ant_hole", MobID::kAntHole},
        {"baby_ant", MobID::kBabyAnt},
        {"worker_ant", MobID::kWorkerAnt},
        {"soldier_ant", MobID::kSoldierAnt},
        {"queen_ant", MobID::kQueenAnt},
        {"bee", MobID::kBee},
        {"ladybug", MobID::kLadybug},
        {"ladybug_shiny", MobID::kShinyLadybug},
        {"ladybug_dark", MobID::kDarkLadybug},
        {"ladybug_massive", MobID::kMassiveLadybug},
        {"shiny_ladybug", MobID::kShinyLadybug},
        {"dark_ladybug", MobID::kDarkLadybug},
        {"massive_ladybug", MobID::kMassiveLadybug},
        {"beetle", MobID::kBeetle},
        {"beetle_massive", MobID::kMassiveBeetle},
        {"massive_beetle", MobID::kMassiveBeetle},
        {"hornet", MobID::kHornet},
        {"cactus", MobID::kCactus},
        {"rock", MobID::kRock},
        {"boulder", MobID::kBoulder},
        {"centipede", MobID::kCentipede},
        {"centipede_evil", MobID::kEvilCentipede},
        {"centipede_desert", MobID::kDesertCentipede},
        {"evil_centipede", MobID::kEvilCentipede},
        {"desert_centipede", MobID::kDesertCentipede},
        {"sandstorm", MobID::kSandstorm},
        {"scorpion", MobID::kScorpion},
        {"spider", MobID::kSpider},
        {"square", MobID::kSquare},
        {"digger", MobID::kDigger},
        {"leafbug", MobID::kLeafbug},
        {"bush", MobID::kBush},
        {"mantis", MobID::kMantis},
        {"wasp", MobID::kWasp},
    };
    auto it = map.find(n);
    return it == map.end() ? MobID::kNumMobs : it->second;
}

// -------- Preset → weighted entries (expand "garden"/"desert"/etc.) ---------

struct PresetEntry { char const *name; float weight; };

std::vector<PresetEntry> const *preset(std::string const &n) {
    static std::vector<PresetEntry> const garden = {
        {"rock", 5}, {"ladybug", 5}, {"bee", 5}, {"hornet", 3}, {"spider", 3},
        {"ant_baby", 1}, {"centipede", 0.5f},
    };
    static std::vector<PresetEntry> const desert = {
        {"cactus", 5}, {"beetle", 3}, {"sandstorm", 1}, {"scorpion", 1},
        {"bee", 1}, {"ladybug", 0.5f}, {"centipede_desert", 0.3f},
    };
    static std::vector<PresetEntry> const ocean = {
        {"rock", 5}, {"boulder", 2}, {"hornet", 2}, {"spider", 2},
        {"ladybug", 1},
    };
    static std::vector<PresetEntry> const jungle = {
        {"ladybug_dark", 5}, {"leafbug", 2}, {"wasp", 2}, {"mantis", 1},
        {"bush", 3}, {"centipede_evil", 0.3f},
    };
    static std::vector<PresetEntry> const ant_hell = {
        {"ant_baby", 3}, {"ant_worker", 5}, {"ant_soldier", 2}, {"ant_hole", 0.5f},
    };
    if (n == "garden") return &garden;
    if (n == "desert") return &desert;
    if (n == "ocean") return &ocean;
    if (n == "jungle") return &jungle;
    if (n == "ant_hell" || n == "anthole" || n == "ant_hole_zone") return &ant_hell;
    return nullptr;
}

// Tiled stores the `mobs` property either as a single preset name ("garden")
// or as a `name:weight;` list. List entries can be preset names too, in which
// case each preset entry's weight is multiplied by the list weight.
std::vector<TiledSpawnEntry> parse_mob_list(std::string const &s) {
    std::vector<TiledSpawnEntry> out;
    auto add = [&](std::string const &name, float weight) {
        if (weight <= 0) return;
        auto const *p = preset(name);
        if (p) {
            for (auto const &pe : *p) {
                MobID::T id = mob_id_from_name(pe.name);
                if (id < MobID::kNumMobs) out.push_back({id, pe.weight * weight});
            }
            return;
        }
        MobID::T id = mob_id_from_name(name);
        if (id < MobID::kNumMobs) out.push_back({id, weight});
    };

    // Bare preset name with no separators.
    bool has_sep = s.find(':') != std::string::npos || s.find(';') != std::string::npos;
    if (!has_sep) { add(s, 1.0f); return out; }

    size_t i = 0;
    while (i < s.size()) {
        // skip whitespace and stray newlines
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
        if (i >= s.size()) break;
        size_t name_start = i;
        while (i < s.size() && s[i] != ':' && s[i] != ';') ++i;
        std::string name = s.substr(name_start, i - name_start);
        // trim trailing whitespace from name
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t' || name.back() == '\n' || name.back() == '\r')) name.pop_back();
        float weight = 1.0f;
        if (i < s.size() && s[i] == ':') {
            ++i;
            size_t num_start = i;
            while (i < s.size() && s[i] != ';') ++i;
            weight = (float)std::strtod(s.substr(num_start, i - num_start).c_str(), nullptr);
        }
        if (i < s.size() && s[i] == ';') ++i;
        if (!name.empty()) add(name, weight);
    }
    return out;
}

// ---------------------------------------------------------------------------
// SVG → polygon. Each "solid" tile (cliff/water/dirt/bush/castle) is a tile
// whose collision shape we want to derive from its SVG markup so the player
// follows the painted edge, not the tile-rectangle. We parse just the
// subset of SVG used by this tileset: <rect> and <path> elements with a
// solid `fill` color, with path-data commands M/m, L/l, H/h, V/v, C/c, S/s,
// and Z/z. Curves are sampled at SVG_CURVE_STEPS points each.
//
// The parser is deliberately minimal — no XML library — because the input
// is a fixed set of hand-authored SVGs whose structure we can rely on.

static constexpr int SVG_CURVE_STEPS = 8;

static void svg_sample_cubic(std::vector<TiledPolyVert> &out,
                             float x0, float y0, float x1, float y1,
                             float x2, float y2, float x3, float y3) {
    for (int i = 1; i <= SVG_CURVE_STEPS; ++i) {
        float t = (float)i / SVG_CURVE_STEPS;
        float mt = 1.0f - t;
        float bx = mt*mt*mt*x0 + 3*mt*mt*t*x1 + 3*mt*t*t*x2 + t*t*t*x3;
        float by = mt*mt*mt*y0 + 3*mt*mt*t*y1 + 3*mt*t*t*y2 + t*t*t*y3;
        out.push_back({bx, by});
    }
}

static bool svg_is_num_char(char c) {
    return (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-' || c == 'e' || c == 'E';
}

static std::vector<TiledPolyVert> svg_parse_path(std::string const &d) {
    std::vector<TiledPolyVert> verts;
    float cpx = 0, cpy = 0;       // current point
    float subx = 0, suby = 0;     // start of current sub-path
    float prev_c2x = 0, prev_c2y = 0; // last cubic's control2, for S/s smooth
    bool have_prev_cubic = false;
    char cmd = 0;
    size_t i = 0;
    const size_t n = d.size();

    auto skip_ws = [&]() {
        while (i < n && (d[i] == ' ' || d[i] == ',' || d[i] == '\t' || d[i] == '\n' || d[i] == '\r')) ++i;
    };
    auto read_num = [&]() -> float {
        skip_ws();
        size_t start = i;
        if (i < n && (d[i] == '+' || d[i] == '-')) ++i;
        bool seen_dot = false;
        bool seen_exp = false;
        while (i < n) {
            char c = d[i];
            if (c >= '0' && c <= '9') { ++i; continue; }
            if (c == '.' && !seen_dot && !seen_exp) { seen_dot = true; ++i; continue; }
            if ((c == 'e' || c == 'E') && !seen_exp) {
                seen_exp = true; ++i;
                if (i < n && (d[i] == '+' || d[i] == '-')) ++i;
                continue;
            }
            break;
        }
        if (i == start) return 0;
        return (float)std::strtod(d.substr(start, i - start).c_str(), nullptr);
    };

    while (i < n) {
        skip_ws();
        if (i >= n) break;
        char c = d[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            cmd = c;
            ++i;
        } else if (!svg_is_num_char(c)) {
            ++i; continue;
        }
        // After M/m the implicit continuation is L/l, per SVG spec.
        char effective_cmd = cmd;
        switch (effective_cmd) {
        case 'M': {
            cpx = read_num(); cpy = read_num();
            subx = cpx; suby = cpy;
            verts.push_back({cpx, cpy});
            cmd = 'L';
            have_prev_cubic = false;
        } break;
        case 'm': {
            cpx += read_num(); cpy += read_num();
            subx = cpx; suby = cpy;
            verts.push_back({cpx, cpy});
            cmd = 'l';
            have_prev_cubic = false;
        } break;
        case 'L': cpx = read_num(); cpy = read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'l': cpx += read_num(); cpy += read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'H': cpx = read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'h': cpx += read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'V': cpy = read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'v': cpy += read_num(); verts.push_back({cpx, cpy}); have_prev_cubic = false; break;
        case 'C': {
            float x1 = read_num(), y1 = read_num();
            float x2 = read_num(), y2 = read_num();
            float x = read_num(),  y = read_num();
            svg_sample_cubic(verts, cpx, cpy, x1, y1, x2, y2, x, y);
            prev_c2x = x2; prev_c2y = y2; have_prev_cubic = true;
            cpx = x; cpy = y;
        } break;
        case 'c': {
            float x1 = read_num() + cpx, y1 = read_num() + cpy;
            float x2 = read_num() + cpx, y2 = read_num() + cpy;
            float x  = read_num() + cpx, y  = read_num() + cpy;
            svg_sample_cubic(verts, cpx, cpy, x1, y1, x2, y2, x, y);
            prev_c2x = x2; prev_c2y = y2; have_prev_cubic = true;
            cpx = x; cpy = y;
        } break;
        case 'S': {
            float x1 = have_prev_cubic ? (2 * cpx - prev_c2x) : cpx;
            float y1 = have_prev_cubic ? (2 * cpy - prev_c2y) : cpy;
            float x2 = read_num(), y2 = read_num();
            float x  = read_num(), y  = read_num();
            svg_sample_cubic(verts, cpx, cpy, x1, y1, x2, y2, x, y);
            prev_c2x = x2; prev_c2y = y2; have_prev_cubic = true;
            cpx = x; cpy = y;
        } break;
        case 's': {
            float x1 = have_prev_cubic ? (2 * cpx - prev_c2x) : cpx;
            float y1 = have_prev_cubic ? (2 * cpy - prev_c2y) : cpy;
            float x2 = read_num() + cpx, y2 = read_num() + cpy;
            float x  = read_num() + cpx, y  = read_num() + cpy;
            svg_sample_cubic(verts, cpx, cpy, x1, y1, x2, y2, x, y);
            prev_c2x = x2; prev_c2y = y2; have_prev_cubic = true;
            cpx = x; cpy = y;
        } break;
        case 'Z': case 'z':
            if (cpx != subx || cpy != suby) verts.push_back({subx, suby});
            cpx = subx; cpy = suby;
            have_prev_cubic = false;
            break;
        default:
            // Unknown command — skip a single number to avoid an infinite
            // loop, then continue looking for a new command letter.
            read_num();
            break;
        }
    }
    return verts;
}

// Return the first element in `svg` that defines a *solid* shape (a path
// or rect with a real `fill` color and no `opacity` attribute). The
// shadow paths in this tileset use `fill="none"` plus a stroke with
// `stroke-opacity`, so the fill-check is enough to skip them.
static std::vector<TiledPolyVert> svg_parse_points(std::string const &s) {
    // points="x,y x,y x,y …" (commas and/or whitespace separators).
    std::vector<TiledPolyVert> out;
    size_t i = 0, n = s.size();
    auto skip = [&]() {
        while (i < n && (s[i] == ' ' || s[i] == ',' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    };
    while (i < n) {
        skip();
        if (i >= n) break;
        size_t start = i;
        if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
        while (i < n) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                ((c == '+' || c == '-') && (s[i-1] == 'e' || s[i-1] == 'E'))) {
                ++i;
            } else break;
        }
        if (i == start) { ++i; continue; }
        float x = (float)std::strtod(s.substr(start, i - start).c_str(), nullptr);
        skip();
        start = i;
        if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
        while (i < n) {
            char c = s[i];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                ((c == '+' || c == '-') && (s[i-1] == 'e' || s[i-1] == 'E'))) {
                ++i;
            } else break;
        }
        if (i == start) break;
        float y = (float)std::strtod(s.substr(start, i - start).c_str(), nullptr);
        out.push_back({x, y});
    }
    return out;
}

static std::vector<TiledPolyVert> svg_extract_polygon(std::string const &svg) {
    size_t pos = 0;
    while (pos < svg.size()) {
        // Find the next supported element. We look for <path, <rect,
        // <polyline, <polygon — Tiled tilesets used in this map mix all
        // three for "the main filled shape". Picking only <path missed
        // dirt_tl_0/dirt_tri_0 (which use <polyline> for the boundary)
        // and fell through to the small decoration <path> dots instead,
        // so the dots were the collision shape rather than the outline.
        size_t a = svg.find("<path", pos);
        size_t b = svg.find("<rect", pos);
        size_t c = svg.find("<polyline", pos);
        size_t d_ = svg.find("<polygon", pos);
        size_t at = std::min(std::min(a, b), std::min(c, d_));
        if (at == std::string::npos) break;
        size_t end = svg.find('>', at);
        if (end == std::string::npos) break;
        std::string elem = svg.substr(at, end - at + 1);

        // What element is this?
        enum { ELT_PATH, ELT_RECT, ELT_POLYLINE, ELT_POLYGON } kind;
        if (elem.compare(1, 4, "path") == 0) kind = ELT_PATH;
        else if (elem.compare(1, 4, "rect") == 0) kind = ELT_RECT;
        else if (elem.compare(1, 8, "polyline") == 0) kind = ELT_POLYLINE;
        else kind = ELT_POLYGON;

        // Extract fill, skip non-solid.
        size_t fp = elem.find("fill=\"");
        if (fp == std::string::npos) { pos = end + 1; continue; }
        size_t fs = fp + 6, fe = elem.find('"', fs);
        std::string fill = elem.substr(fs, fe - fs);
        if (fill == "none" || fill.empty()) { pos = end + 1; continue; }

        // Skip elements with `opacity="..."` — those are shadows.
        if (elem.find("opacity=\"") != std::string::npos) { pos = end + 1; continue; }

        if (kind == ELT_PATH) {
            size_t dp = elem.find("d=\"");
            if (dp == std::string::npos) { pos = end + 1; continue; }
            size_t ds = dp + 3, de = elem.find('"', ds);
            std::string d = elem.substr(ds, de - ds);
            auto poly = svg_parse_path(d);
            if (!poly.empty()) return poly;
        } else if (kind == ELT_POLYLINE || kind == ELT_POLYGON) {
            size_t pp = elem.find("points=\"");
            if (pp == std::string::npos) { pos = end + 1; continue; }
            size_t ps = pp + 8, pe = elem.find('"', ps);
            std::string pts = elem.substr(ps, pe - ps);
            auto poly = svg_parse_points(pts);
            if (poly.size() >= 3) return poly;
        } else {
            // <rect> — read x/y/width/height, default x=0 y=0.
            auto attr = [&](char const *k) -> float {
                std::string key = std::string(k) + "=\"";
                size_t kp = elem.find(key);
                if (kp == std::string::npos) return 0;
                size_t ks = kp + key.size();
                size_t ke = elem.find('"', ks);
                return (float)std::strtod(elem.substr(ks, ke - ks).c_str(), nullptr);
            };
            float x = attr("x"), y = attr("y"), w = attr("width"), h = attr("height");
            if (w <= 0 || h <= 0) { pos = end + 1; continue; }
            std::vector<TiledPolyVert> poly = {
                {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}
            };
            return poly;
        }
        pos = end + 1;
    }
    return {};
}

// Load and cache the polygon for a tile by SVG filename. Returns nullptr
// if the file is missing or has no solid shape.
static std::vector<TiledPolyVert> const *load_tile_shape(uint32_t base_gid,
                                                         std::string const &svg_path) {
    auto it = TiledMap::tile_shape_cache.find(base_gid);
    if (it != TiledMap::tile_shape_cache.end()) return &it->second;

    std::ifstream f(svg_path);
    if (!f) {
        std::cerr << "[TiledMap] could not open " << svg_path << " for gid " << base_gid << "\n";
        return nullptr;
    }
    std::stringstream ss; ss << f.rdbuf();
    auto poly = svg_extract_polygon(ss.str());
    if (poly.empty()) {
        std::cerr << "[TiledMap] no solid path in " << svg_path << " for gid " << base_gid << "\n";
        return nullptr;
    }
    auto [it2, _] = TiledMap::tile_shape_cache.emplace(base_gid, std::move(poly));
    return &it2->second;
}

// Apply the cell's flip flags to a polygon vertex expressed in image-local
// pixel coords (0..tile_w_image × 0..tile_h_image). The output is in the
// same coord space — still 0..image_w × 0..image_h — but flipped per the
// Tiled CellRenderer convention (see Client/Render/TiledMapRender.cc for
// the matching rendering math).
static TiledPolyVert apply_flip(float u, float v, float iw, float ih,
                                bool fH, bool fV, bool fD) {
    if (fD) {
        // Tiled: rotate 90° CW about tile center, then scale per
        // newH=origV, newV=!origH. The composite, with all coords
        // measured from TL with positive Y down, simplifies to:
        //   sx = newH ? -1 : 1
        //   sy = newV ? -1 : 1
        // applied to the rotated point (iw - v, u) — except that
        // because the rotation is around the *center* (iw/2, ih/2),
        // a scale of -1 on the rotated axis maps back to: cell_x =
        // 0.5*iw + sy*(0.5*iw - v) and cell_y = 0.5*ih + sx*(u - 0.5*ih).
        float sx = fV ? -1.0f : 1.0f;
        float sy = (!fH) ? -1.0f : 1.0f;
        return { 0.5f * iw + sy * (0.5f * iw - v),
                 0.5f * ih + sx * (u - 0.5f * ih) };
    } else {
        return { fH ? (iw - u) : u, fV ? (ih - v) : v };
    }
}

// Build a per-cell world-space polygon from the tile's local polygon,
// applying the cell's flip flags and translating to its grid position.
// SVG coords are 0..256 (image space); we scale to 0..tile_w world.
static void build_cell_polygon(uint32_t col, uint32_t row,
                               uint32_t tile_w, uint32_t tile_h,
                               std::vector<TiledPolyVert> const &local_poly,
                               uint32_t raw_gid,
                               float image_w, float image_h) {
    bool fH = (raw_gid & 0x80000000u) != 0;
    bool fV = (raw_gid & 0x40000000u) != 0;
    bool fD = (raw_gid & 0x20000000u) != 0;

    TiledCollisionPoly out;
    out.verts.reserve(local_poly.size());
    out.min_x = out.min_y =  1e30f;
    out.max_x = out.max_y = -1e30f;
    float sx_world = (float)tile_w / image_w;
    float sy_world = (float)tile_h / image_h;
    for (auto const &p : local_poly) {
        TiledPolyVert flipped = apply_flip(p.x, p.y, image_w, image_h, fH, fV, fD);
        float wx = col * (float)tile_w + flipped.x * sx_world;
        float wy = row * (float)tile_h + flipped.y * sy_world;
        out.verts.push_back({wx, wy});
        if (wx < out.min_x) out.min_x = wx;
        if (wy < out.min_y) out.min_y = wy;
        if (wx > out.max_x) out.max_x = wx;
        if (wy > out.max_y) out.max_y = wy;
    }
    if (out.verts.size() >= 3) TiledMap::collision_polys.push_back(std::move(out));
}

// ---------------------------------------------------------------------------
// Base64 + gzip helpers for the tile-layer data. Tiled stores each tile
// layer as a base64-encoded gzip stream of uint32 GIDs; we only care about
// "is this cell non-zero" so we can derive collision rects from solid tile
// types (cliff, water).

static bool base64_decode(std::string const &s, std::vector<uint8_t> &out) {
    static int8_t const tbl[256] = {
        // 256-byte lookup, -1 for invalid. Init via designators wouldn't
        // be portable to all C++20 frontends so fill at runtime instead.
    };
    static bool initialized = false;
    static int8_t lookup[256];
    if (!initialized) {
        for (int i = 0; i < 256; ++i) lookup[i] = -1;
        char const *a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) lookup[(uint8_t)a[i]] = (int8_t)i;
        initialized = true;
    }
    (void)tbl;
    out.clear();
    out.reserve(s.size() * 3 / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int8_t v = lookup[(uint8_t)c];
        if (v < 0) return false;
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((buf >> bits) & 0xff));
        }
    }
    return true;
}

static bool gunzip(std::vector<uint8_t> const &in, std::vector<uint8_t> &out) {
    z_stream z{};
    z.next_in = (Bytef *)in.data();
    z.avail_in = (uInt)in.size();
    // 16 + MAX_WBITS = gzip-only decoding (zlib's documented incantation).
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) return false;
    uint8_t chunk[16 * 1024];
    int ret;
    do {
        z.next_out = chunk;
        z.avail_out = sizeof(chunk);
        ret = inflate(&z, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&z);
            return false;
        }
        out.insert(out.end(), chunk, chunk + (sizeof(chunk) - z.avail_out));
    } while (ret != Z_STREAM_END);
    inflateEnd(&z);
    return true;
}

// Parse a `tilelayer` and, if its name is in `solid_layers`, emit one
// collision rect for every non-zero cell. Tiles are 512×512 in world
// units (set by `tilewidth` in the .tmj). We strip the upper 3 flip bits
// before checking; flip flags don't change whether a cell is solid.
// gid → svg filename. Built once from the tileset JSON when the map loads.
static std::unordered_map<uint32_t, std::string> g_gid_to_svg;
// Tile image native size (square in this tileset); used to scale image
// coords to world coords.
static float g_image_w = 256.0f;
static float g_image_h = 256.0f;
// Directory the SVGs live in. We need to fetch this when we know the
// tileset's source path.
static std::string g_tileset_dir;

static std::string resolve_tileset_path(std::string const &map_path,
                                        std::string const &src) {
    // Tiled writes the tileset source as a path relative to the .tmj.
    // Most often "../../tiles/tileset.tsj" for this map.
    if (src.empty()) return {};
    if (src[0] == '/') return src;
    size_t s = map_path.find_last_of("/\\");
    std::string dir = (s == std::string::npos) ? "" : map_path.substr(0, s + 1);
    std::string combined = dir + src;
    // Resolve ".." segments.
    std::vector<std::string> parts;
    size_t i = 0;
    while (i < combined.size()) {
        size_t j = combined.find('/', i);
        if (j == std::string::npos) j = combined.size();
        std::string part = combined.substr(i, j - i);
        if (part == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (!part.empty() && part != ".") parts.push_back(part);
        i = j + 1;
    }
    std::string out;
    if (!combined.empty() && combined[0] == '/') out = "/";
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k) out += '/';
        out += parts[k];
    }
    return out;
}

static void load_tileset(std::string const &map_path, Json const &map_root) {
    g_gid_to_svg.clear();
    Json const *tilesets = map_root.find("tilesets");
    if (!tilesets || tilesets->type != Json::kArr || tilesets->arr.empty()) return;
    Json const *ts_ref = &tilesets->arr[0];
    int firstgid = (int)(ts_ref->find("firstgid") ? ts_ref->find("firstgid")->as_num() : 1);
    Json const *src = ts_ref->find("source");
    if (!src || src->type != Json::kStr) return;
    std::string ts_path = resolve_tileset_path(map_path, src->as_str());
    std::ifstream f(ts_path);
    if (!f) {
        std::cerr << "[TiledMap] could not open tileset " << ts_path << "\n";
        return;
    }
    size_t slash = ts_path.find_last_of("/\\");
    g_tileset_dir = (slash == std::string::npos) ? "" : ts_path.substr(0, slash + 1);

    std::stringstream ss; ss << f.rdbuf();
    std::string text = ss.str();
    try {
        Parser parser(text.data(), text.data() + text.size());
        Json ts = parser.parse();
        Json const *tw = ts.find("tilewidth");
        Json const *th = ts.find("tileheight");
        if (tw) g_image_w = (float)tw->as_num();
        if (th) g_image_h = (float)th->as_num();
        Json const *tiles = ts.find("tiles");
        if (!tiles || tiles->type != Json::kArr) return;
        for (auto const &t : tiles->arr) {
            Json const *id = t.find("id");
            Json const *img = t.find("image");
            if (!id || !img) continue;
            uint32_t gid = (uint32_t)(firstgid + (int)id->as_num());
            g_gid_to_svg[gid] = img->as_str();
        }
    } catch (std::exception const &e) {
        std::cerr << "[TiledMap] tileset parse error: " << e.what() << "\n";
    }
}

void parse_solid_tile_layer(Json const &layer, uint32_t tile_w, uint32_t tile_h) {
    Json const *encoding = layer.find("encoding");
    Json const *compression = layer.find("compression");
    Json const *data = layer.find("data");
    Json const *widthJ = layer.find("width");
    if (!data || data->type != Json::kStr) return;
    if (!widthJ) return;
    if (!encoding || encoding->as_str() != "base64") return;
    if (!compression || compression->as_str() != "gzip") return;

    std::vector<uint8_t> raw;
    if (!base64_decode(data->str, raw)) return;
    std::vector<uint8_t> gids_bytes;
    if (!gunzip(raw, gids_bytes)) return;
    if (gids_bytes.size() % 4 != 0) return;

    uint32_t width = (uint32_t)widthJ->as_num();
    if (width == 0) return;
    uint32_t count = (uint32_t)(gids_bytes.size() / 4);
    uint32_t height = count / width;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t raw_gid =
            (uint32_t)gids_bytes[i * 4 + 0] |
            ((uint32_t)gids_bytes[i * 4 + 1] << 8) |
            ((uint32_t)gids_bytes[i * 4 + 2] << 16) |
            ((uint32_t)gids_bytes[i * 4 + 3] << 24);
        uint32_t base_gid = raw_gid & 0x1fffffff;
        if (!base_gid) continue;
        uint32_t col = i % width;
        uint32_t row = i / width;
        if (row >= height) break;

        // Try to load this tile's SVG polygon; fall back to a full-tile
        // rectangle if the SVG file is missing or has no solid shape.
        auto svg_it = g_gid_to_svg.find(base_gid);
        std::vector<TiledPolyVert> const *poly = nullptr;
        if (svg_it != g_gid_to_svg.end()) {
            poly = load_tile_shape(base_gid, g_tileset_dir + svg_it->second);
        }
        if (poly) {
            build_cell_polygon(col, row, tile_w, tile_h, *poly, raw_gid, g_image_w, g_image_h);
        } else {
            TiledCollisionRect r;
            r.x = (float)(col * tile_w);
            r.y = (float)(row * tile_h);
            r.w = (float)tile_w;
            r.h = (float)tile_h;
            TiledMap::collision_rects.push_back(r);
        }
    }
}

void parse_collision_layer(Json const &layer) {
    Json const *objs = layer.find("objects");
    if (!objs || objs->type != Json::kArr) return;
    for (auto const &o : objs->arr) {
        Json const *typ = o.find("type");
        if (!typ || typ->as_str() != "collision") continue;
        TiledCollisionRect r;
        r.x = (float)(o.find("x") ? o.find("x")->as_num() : 0);
        r.y = (float)(o.find("y") ? o.find("y")->as_num() : 0);
        r.w = (float)(o.find("width") ? o.find("width")->as_num() : 0);
        r.h = (float)(o.find("height") ? o.find("height")->as_num() : 0);
        if (r.w > 0 && r.h > 0) TiledMap::collision_rects.push_back(r);
    }
}

void parse_mobs_layer(Json const &layer) {
    Json const *objs = layer.find("objects");
    if (!objs || objs->type != Json::kArr) return;
    for (auto const &o : objs->arr) {
        Json const *typ = o.find("type");
        if (!typ || typ->as_str() != "spawn_mobs") continue;
        Json const *poly = o.find("polygon");
        if (!poly || poly->type != Json::kArr || poly->arr.size() < 3) continue;
        float ox = (float)(o.find("x") ? o.find("x")->as_num() : 0);
        float oy = (float)(o.find("y") ? o.find("y")->as_num() : 0);

        TiledSpawnPolygon p;
        p.density = 1.0f;
        std::string mobs_str;
        if (Json const *props = o.find("properties")) {
            if (props->type == Json::kArr) {
                for (auto const &pr : props->arr) {
                    Json const *name = pr.find("name");
                    Json const *val = pr.find("value");
                    if (!name || !val) continue;
                    if (name->as_str() == "density") p.density = (float)val->as_num(1.0);
                    else if (name->as_str() == "mobs") mobs_str = val->as_str();
                }
            }
        }
        p.spawns = parse_mob_list(mobs_str);
        if (p.spawns.empty()) continue;

        p.min_x = p.min_y = 1e30f; p.max_x = p.max_y = -1e30f;
        for (auto const &v : poly->arr) {
            float vx = ox + (float)(v.find("x") ? v.find("x")->as_num() : 0);
            float vy = oy + (float)(v.find("y") ? v.find("y")->as_num() : 0);
            p.vx.push_back(vx);
            p.vy.push_back(vy);
            if (vx < p.min_x) p.min_x = vx;
            if (vy < p.min_y) p.min_y = vy;
            if (vx > p.max_x) p.max_x = vx;
            if (vy > p.max_y) p.max_y = vy;
        }
        // shoelace area (absolute)
        double a = 0;
        size_t n = p.vx.size();
        for (size_t i = 0; i < n; ++i) {
            size_t j = (i + 1) % n;
            a += (double)p.vx[i] * p.vy[j] - (double)p.vx[j] * p.vy[i];
        }
        p.area = (float)std::fabs(a * 0.5);
        if (p.area < 1.0f) continue;
        TiledMap::spawn_polygons.push_back(std::move(p));
    }
}

} // namespace

namespace TiledMap {

bool point_in_polygon(TiledSpawnPolygon const &poly, float x, float y) {
    if (x < poly.min_x || x > poly.max_x || y < poly.min_y || y > poly.max_y) return false;
    bool inside = false;
    size_t n = poly.vx.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = poly.vx[i], yi = poly.vy[i];
        float xj = poly.vx[j], yj = poly.vy[j];
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / (yj - yi + 1e-9f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Test whether (px, py) lies inside a simple polygon via the classic
// ray-casting odd-crossings test.
static bool point_in_simple_poly(std::vector<TiledPolyVert> const &v, float px, float py) {
    bool inside = false;
    size_t n = v.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = v[i].x, yi = v[i].y;
        float xj = v[j].x, yj = v[j].y;
        bool intersect = ((yi > py) != (yj > py)) &&
                         (px < (xj - xi) * (py - yi) / (yj - yi + 1e-9f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Closest point on segment (a, b) to point p. Returns distance² and writes
// the closest point into `out_x, out_y` and the outward direction (from
// the segment toward p) into `out_nx, out_ny` (unit, with magnitude 1 — or
// zero if p is exactly on the segment).
static float closest_on_segment(float ax, float ay, float bx, float by,
                                float px, float py,
                                float &out_x, float &out_y,
                                float &out_nx, float &out_ny) {
    float dx = bx - ax, dy = by - ay;
    float len2 = dx * dx + dy * dy;
    float t = (len2 > 1e-9f) ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    out_x = ax + t * dx;
    out_y = ay + t * dy;
    float ndx = px - out_x, ndy = py - out_y;
    float nd2 = ndx * ndx + ndy * ndy;
    if (nd2 > 1e-9f) {
        float nd = std::sqrt(nd2);
        out_nx = ndx / nd;
        out_ny = ndy / nd;
    } else {
        out_nx = 0; out_ny = 0;
    }
    return nd2;
}

void resolve_collision(float &x, float &y, float radius) {
    if (!loaded) return;

    // Authored `collision` objectgroup → simple AABB push-out.
    for (auto const &r : collision_rects) {
        float l = r.x - radius;
        float t = r.y - radius;
        float ri = r.x + r.w + radius;
        float bo = r.y + r.h + radius;
        if (x <= l || x >= ri || y <= t || y >= bo) continue;
        float dl = x - l, dr = ri - x, dt = y - t, db = bo - y;
        float m = dl;
        int axis = 0;
        if (dr < m) { m = dr; axis = 1; }
        if (dt < m) { m = dt; axis = 2; }
        if (db < m) { m = db; axis = 3; }
        switch (axis) {
            case 0: x = l; break;
            case 1: x = ri; break;
            case 2: y = t; break;
            case 3: y = bo; break;
        }
    }

    // Per-cell SVG-derived polygons → circle-vs-polygon push-out.
    // For each polygon overlapping the entity's AABB, find the closest
    // point on the polygon's edge ring; if the entity penetrates it,
    // push out along the outward normal of that edge by the penetration
    // depth. Handles two cases:
    //   1. Center outside but within radius of an edge → push outward
    //      along (center − closest_point).
    //   2. Center inside polygon (deep penetration) → push toward the
    //      nearest edge so the entity ends up just outside with clearance.
    for (auto const &p : collision_polys) {
        // Early-out via expanded bbox.
        if (x < p.min_x - radius || x > p.max_x + radius ||
            y < p.min_y - radius || y > p.max_y + radius) continue;

        // Find the closest point on the polygon edge ring.
        float best_d2 = 1e30f;
        float best_cx = 0, best_cy = 0;
        float best_nx = 0, best_ny = 0;
        size_t n = p.verts.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            float cx, cy, nx, ny;
            float d2 = closest_on_segment(p.verts[j].x, p.verts[j].y,
                                          p.verts[i].x, p.verts[i].y,
                                          x, y, cx, cy, nx, ny);
            if (d2 < best_d2) {
                best_d2 = d2;
                best_cx = cx; best_cy = cy;
                best_nx = nx; best_ny = ny;
            }
        }

        bool inside = point_in_simple_poly(p.verts, x, y);
        float best_d = std::sqrt(best_d2);

        if (inside) {
            // Push from the closest edge outward by radius (so the
            // entity ends up at least `radius` away from the wall). The
            // outward normal is from the closest point toward the
            // center — but center is inside the polygon, so that vector
            // points *into* the polygon. Flip it.
            float nx = -best_nx, ny = -best_ny;
            if (nx == 0 && ny == 0) {
                // Pathological: center exactly on edge. Pick any axis.
                nx = 1; ny = 0;
            }
            x = best_cx + nx * radius;
            y = best_cy + ny * radius;
        } else if (best_d < radius) {
            // Center outside but within radius → push along outward
            // normal until the entity's surface just touches the edge.
            float push = radius - best_d;
            x += best_nx * push;
            y += best_ny * push;
        }
    }
}

bool load(std::string const &path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[TiledMap] could not open " << path << "\n";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();

    collision_rects.clear();
    collision_polys.clear();
    tile_shape_cache.clear();
    spawn_polygons.clear();

    try {
        Parser parser(text.data(), text.data() + text.size());
        Json root = parser.parse();
        // Resolve the tileset and build the gid→SVG map first; the
        // solid-tile layers need it to produce per-cell collision polygons.
        load_tileset(path, root);
        Json const *layers = root.find("layers");
        if (!layers || layers->type != Json::kArr) {
            std::cerr << "[TiledMap] no layers in " << path << "\n";
            return false;
        }
        uint32_t tile_w = (uint32_t)(root.find("tilewidth") ? root.find("tilewidth")->as_num() : 512);
        uint32_t tile_h = (uint32_t)(root.find("tileheight") ? root.find("tileheight")->as_num() : 512);
        for (auto const &layer : layers->arr) {
            Json const *type = layer.find("type");
            Json const *name = layer.find("name");
            if (!type || !name) continue;
            if (type->as_str() == "objectgroup") {
                if (name->as_str() == "collision") parse_collision_layer(layer);
                else if (name->as_str() == "mobs") parse_mobs_layer(layer);
            } else if (type->as_str() == "tilelayer") {
                // Layers whose tiles act as walls. The authored
                // `collision` objectgroup only covers a few special
                // spots; everything visible-looking-solid (cliff faces,
                // ponds, dirt embankments, castle walls) needs to
                // block movement too, or the player walks straight
                // through what looks like a wall.
                std::string const &n = name->as_str();
                if (n == "cliff" || n == "water" || n == "dirt" || n == "castle" || n == "bush") {
                    parse_solid_tile_layer(layer, tile_w, tile_h);
                }
            }
        }
    } catch (std::exception const &e) {
        std::cerr << "[TiledMap] parse error: " << e.what() << "\n";
        return false;
    }

    mob_counts.assign(spawn_polygons.size(), 0);
    loaded = true;
    std::cout << "[TiledMap] loaded " << path
              << " — " << spawn_polygons.size() << " spawn polygons, "
              << collision_rects.size() << " collision rects, "
              << collision_polys.size() << " collision polygons\n";
    return true;
}

bool spawn_random_mob(Simulation *sim) {
    if (!loaded || spawn_polygons.empty()) return false;

    // Pick a polygon weighted by area × density, but reject any polygon
    // that has hit its density cap (area × density / (500×500)).
    double total = 0;
    std::vector<double> cum(spawn_polygons.size(), 0);
    for (size_t i = 0; i < spawn_polygons.size(); ++i) {
        auto const &p = spawn_polygons[i];
        double cap = (double)p.density * p.area / (500.0 * 500.0);
        if ((double)mob_counts[i] >= cap) { cum[i] = total; continue; }
        total += (double)p.area * p.density;
        cum[i] = total;
    }
    if (total <= 0) return true; // nothing to spawn into right now

    double pick = frand() * total;
    size_t idx = 0;
    for (; idx < cum.size(); ++idx) if (pick <= cum[idx]) break;
    if (idx >= spawn_polygons.size()) return true;
    auto const &p = spawn_polygons[idx];

    // Rejection sampling for a point inside the polygon. The bounding box
    // is the worst case here; for very thin polygons we may need many
    // tries — cap at 16 attempts and give up if it fails.
    float x = 0, y = 0;
    bool ok = false;
    for (int tries = 0; tries < 16; ++tries) {
        x = p.min_x + frand() * (p.max_x - p.min_x);
        y = p.min_y + frand() * (p.max_y - p.min_y);
        if (point_in_polygon(p, x, y)) { ok = true; break; }
    }
    if (!ok) return true;

    // Avoid spawning inside a wall.
    float r = 30;
    float ox = x, oy = y;
    resolve_collision(x, y, r);
    if (x != ox || y != oy) {
        // If collision shoved us out of the polygon, skip this tick.
        if (!point_in_polygon(p, x, y)) return true;
    }

    // Weighted roll over mob entries.
    double sum = 0;
    for (auto const &s : p.spawns) sum += s.chance;
    if (sum <= 0) return true;
    double r2 = frand() * sum;
    MobID::T chosen = p.spawns.back().id;
    for (auto const &s : p.spawns) {
        r2 -= s.chance;
        if (r2 <= 0) { chosen = s.id; break; }
    }

    Entity &ent = alloc_mob(sim, chosen, x, y, NULL_ENTITY);
    ent.zone = (uint8_t)(idx < 255 ? idx : 255);
    ent.immunity_ticks = SIM_RATE;
    BIT_SET(ent.flags, EntityFlags::kSpawnedFromZone);
    mob_counts[idx]++;
    return true;
}

void note_mob_death(uint32_t poly_idx) {
    if (poly_idx >= mob_counts.size()) return;
    if (mob_counts[poly_idx] > 0) --mob_counts[poly_idx];
}

} // namespace TiledMap
