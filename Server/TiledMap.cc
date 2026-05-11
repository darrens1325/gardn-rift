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
    std::vector<TiledSpawnPolygon> spawn_polygons;
    static std::vector<uint32_t> mob_counts;
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
        uint32_t gid =
            (uint32_t)gids_bytes[i * 4 + 0] |
            ((uint32_t)gids_bytes[i * 4 + 1] << 8) |
            ((uint32_t)gids_bytes[i * 4 + 2] << 16) |
            ((uint32_t)gids_bytes[i * 4 + 3] << 24);
        gid &= 0x1fffffff; // strip flip flags
        if (!gid) continue;
        uint32_t col = i % width;
        uint32_t row = i / width;
        if (row >= height) break;
        TiledCollisionRect r;
        r.x = (float)(col * tile_w);
        r.y = (float)(row * tile_h);
        r.w = (float)tile_w;
        r.h = (float)tile_h;
        TiledMap::collision_rects.push_back(r);
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

void resolve_collision(float &x, float &y, float radius) {
    if (!loaded) return;
    for (auto const &r : collision_rects) {
        // Treat the rectangle as expanded by `radius` so the entity's
        // body — not just its center — stays outside.
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
    spawn_polygons.clear();

    try {
        Parser parser(text.data(), text.data() + text.size());
        Json root = parser.parse();
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
              << collision_rects.size() << " collision rects\n";
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
