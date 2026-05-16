#include <Client/Render/TiledMapRender.hh>

#include <Client/Render/Renderer.hh>

#include <emscripten.h>

// EM_JS used here (not EM_ASM) because both bodies contain commas at the
// JS top level — array literals, multiple destructured locals, etc. The
// C preprocessor splits EM_ASM's first argument on those commas; EM_JS's
// variadic tail collects them into a single body.
//
// IMPORTANT: every property read on a JSON-parsed object (or on the
// Module global) uses *quoted* property access — `obj["foo"]` rather
// than `obj.foo`. Emscripten's `--closure=1` post-pass renames unquoted
// property names; the bundle would happily reference `e.M` while the
// real JSON only has `layers`. Quoted accesses are treated as literal
// strings by Closure and survive minification. (We previously shipped a
// version that read `mapJson.layers` and got back undefined for every
// tilelayer/tileset — symptoms: "0 tile layers, 0 tile images".)

EM_JS(void, tiled_map_init_js, (), {
    if (Module["tiledMap"]) return;
    // Use *quoted* property names in this literal — Closure renames
    // unquoted ones, but our subsequent reads (M["layers"], etc.) are
    // quoted, so they'd look up the original names on a renamed object
    // and get back undefined.
    Module["tiledMap"] = { "ready": false, "layers": [], "tileImages": {}, "objects": [], "warps": [] };

    const M = Module["tiledMap"];

    // Resolve `rel` relative to dir(base) as an absolute MEMFS path.
    // Used to mirror the URL-based tileset/image resolution against the
    // --embed-file MEMFS layout (Map/main/main.tmj → /Map/main/main.tmj,
    // tileset "../../tiles/tileset.tsj" → /tiles/tileset.tsj).
    function memfsResolve(base, rel) {
        const baseDir = base.substring(0, base.lastIndexOf("/"));
        const parts = (baseDir + "/" + rel).split("/");
        const out = [];
        for (let i = 0; i < parts.length; i++) {
            const p = parts[i];
            if (p === "" || p === ".") continue;
            if (p === "..") { out.pop(); continue; }
            out.push(p);
        }
        return "/" + out.join("/");
    }

    // Read an asset as text. In the bundle build (--embed-file) the
    // file is in MEMFS and we read it directly — required because
    // `fetch()` against a file:// page hits CORS, and a deployed page
    // typically doesn't ship the .tmj/.svg files alongside it (404).
    // In the standalone client, MEMFS has no embed so the readFile
    // throws and we fall back to the normal HTTP fetch.
    async function readAssetText(memPath, urlPath) {
        if (Module["FS"]) {
            try { return Module["FS"].readFile(memPath, { "encoding": "utf8" }); }
            catch (e) { /* not in MEMFS; fall through to fetch */ }
        }
        const r = await fetch(urlPath);
        if (!r.ok) throw new Error("http " + r.status + " for " + urlPath);
        return r.text();
    }

    async function readAssetTextAny(paths) {
        let lastError = null;
        for (let i = 0; i < paths.length; i++) {
            try { return await readAssetText(paths[i][0], paths[i][1]); }
            catch (e) { lastError = e; }
        }
        throw lastError || new Error("no asset candidates");
    }

    // Binary equivalent for bitmap tiles (PNG/JPG/etc.). Reading these
    // as utf-8 text — what readAssetText does — silently mangles the
    // bytes and the resulting Image fails to decode. MEMFS readFile
    // returns a Uint8Array when called without an encoding option.
    async function readAssetBinary(memPath, urlPath) {
        if (Module["FS"]) {
            try { return Module["FS"].readFile(memPath); }
            catch (e) { /* not in MEMFS; fall through to fetch */ }
        }
        const r = await fetch(urlPath);
        if (!r.ok) throw new Error("http " + r.status + " for " + urlPath);
        const buf = await r.arrayBuffer();
        return new Uint8Array(buf);
    }

    function mimeFromExt(name) {
        const i = name.lastIndexOf(".");
        const ext = i >= 0 ? name.substring(i + 1).toLowerCase() : "";
        if (ext === "svg") return "image/svg+xml";
        if (ext === "png") return "image/png";
        if (ext === "jpg" || ext === "jpeg") return "image/jpeg";
        if (ext === "gif") return "image/gif";
        if (ext === "webp") return "image/webp";
        if (ext === "bmp") return "image/bmp";
        return "";
    }

    function uniquePush(arr, memPath, urlPath) {
        for (let i = 0; i < arr.length; i++)
            if (arr[i][0] === memPath && arr[i][1] === urlPath) return;
        arr.push([memPath, urlPath]);
    }

    function tilesetPathCandidates(memPath, urlPath) {
        const out = [];
        function addVariants(m, u) {
            if (m.endsWith(".tsx") || u.endsWith(".tsx"))
                uniquePush(out,
                    m.endsWith(".tsx") ? m.substring(0, m.length - 4) + ".tsj" : m,
                    u.endsWith(".tsx") ? u.substring(0, u.length - 4) + ".tsj" : u);
            uniquePush(out, m, u);
        }
        addVariants(memPath, urlPath);
        const mapTiles = "/Map/tiles/";
        if (memPath.indexOf(mapTiles) === 0)
            addVariants("/tiles/" + memPath.substring(mapTiles.length), "/tiles/" + memPath.substring(mapTiles.length));
        return out;
    }

    // Load every external tileset the .tmj references, merging their tiles
    // into M["tileImages"] keyed by absolute gid (firstgid + tile.id). Maps
    // with multiple tilesets used to silently lose every tile from
    // tilesets[1..] — this loops the array and lets a single tileset failure
    // log without aborting the rest.
    async function loadTilesets(mapJson, mapPath) {
        const tsList = mapJson["tilesets"] || [];
        if (!tsList.length) { console.warn("[TiledMap] map has no tilesets"); return; }
        for (let ti = 0; ti < tsList.length; ti++) {
            try { await loadOneTileset(tsList[ti], mapPath); }
            catch (e) { console.warn("[TiledMap] tileset " + ti + " failed: " + e.message); }
        }
    }

    async function loadOneTileset(tsRef, mapPath) {
        const firstgid = (tsRef["firstgid"] | 0);
        const mapDir = mapPath.substring(0, mapPath.lastIndexOf("/"));
        const baseHref = location.href.replace(/[^/]*$/, "");
        const tsUrl = new URL(tsRef["source"], baseHref + mapDir + "/");
        const tsPath = tsUrl.pathname;
        const tsMemPath = memfsResolve("/" + mapPath, tsRef["source"]);
        console.log("[TiledMap] loading tileset " + tsMemPath + " (url " + tsPath + ")");
        const tsText = await readAssetTextAny(tilesetPathCandidates(tsMemPath, tsPath));
        const ts = JSON.parse(tsText);
        const tsDir = tsPath.substring(0, tsPath.lastIndexOf("/"));
        const tsMemDir = tsMemPath.substring(0, tsMemPath.lastIndexOf("/"));
        const tilesArr = ts["tiles"] || [];
        // Why we don't just `img.src = url` here:
        // The shipped SVGs have a root element of `<svg xml:space="preserve"
        // viewBox="…">` with no `xmlns="http://www.w3.org/2000/svg"`.
        // Browsers will happily inline that into HTML, but they refuse to
        // load it through an <img> (the namespace check is strict for
        // external SVGs) and fire `error`. Workaround: fetch each SVG as
        // text, inject the namespace if missing, then hand the patched
        // markup to the Image via a data: URL.
        const ready = (M["tileReady"] = M["tileReady"] || {});
        const names = (M["tileNames"] = M["tileNames"] || {});
        const tileShapes = (M["tileShapes"] = M["tileShapes"] || {});
        const tileImageSize = (M["tileImageSize"] = M["tileImageSize"] || {});
        for (let i = 0; i < tilesArr.length; i++) {
            const t = tilesArr[i];
            const gid = firstgid + ((t["id"] | 0));
            const url = tsDir + "/" + t["image"];
            const memUrl = memfsResolve(tsMemDir + "/_", t["image"]);
            const img = new Image();
            M["tileImages"][gid] = img;
            names[gid] = t["image"];
            // Capture this tile's per-tile collision shapes (the boxes
            // and polygons that show up in the Tiled editor as the
            // tile's "collision" objectgroup). These are in image-local
            // pixel coords against (imagewidth, imageheight).
            const og = t["objectgroup"];
            if (og && og["objects"]) {
                const objs = og["objects"];
                const shapes = [];
                for (let oi = 0; oi < objs.length; oi++) {
                    const o = objs[oi];
                    const ox = +o["x"] || 0;
                    const oy = +o["y"] || 0;
                    if (o["polygon"]) {
                        const poly = o["polygon"];
                        const verts = [];
                        for (let pi = 0; pi < poly.length; pi++)
                            verts.push({ x: ox + (+poly[pi]["x"] || 0), y: oy + (+poly[pi]["y"] || 0) });
                        if (verts.length >= 3) shapes.push({ type: "poly", verts: verts });
                    } else {
                        // Rect (and ellipse fallback) → 4 corners. A
                        // flipped rect can rotate corner order, so we
                        // always emit it as a polygon to share code.
                        const ow = +o["width"] || 0;
                        const oh = +o["height"] || 0;
                        if (ow > 0 && oh > 0) {
                            shapes.push({ type: "poly", verts: [
                                { x: ox,      y: oy      },
                                { x: ox + ow, y: oy      },
                                { x: ox + ow, y: oy + oh },
                                { x: ox,      y: oy + oh }
                            ]});
                        }
                    }
                }
                if (shapes.length) tileShapes[gid] = shapes;
            }
            tileImageSize[gid] = {
                w: +t["imagewidth"] || 0,
                h: +t["imageheight"] || 0
            };
            const mime = mimeFromExt(t["image"]);
            (function (g, im, u, mu, mt) {
                im.addEventListener("load", function () { ready[g] = true; });
                im.addEventListener("error", function () { ready[g] = false; });
                if (mt === "image/svg+xml" || mt === "") {
                    // SVG path: fetch as text and inject the xmlns
                    // attribute if missing — the shipped SVGs omit it
                    // and browsers refuse to decode them through <img>
                    // without it. Also covers extension-less files by
                    // assuming SVG (the historical default here).
                    readAssetText(mu, u)
                        .then(function (txt) {
                            // \\s in C source → \s in the JS regex at runtime.
                            // (Single-backslash \s isn't a recognised C escape;
                            // the preprocessor drops the backslash so the regex
                            // becomes /<svg(s|>)/ which never matches anything
                            // useful. Same warning trap I hit earlier with \/.)
                            if (txt.indexOf("xmlns=") < 0) {
                                txt = txt.replace(/<svg(\\s|>)/, '<svg xmlns="http://www.w3.org/2000/svg"$1');
                            }
                            im.src = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(txt);
                        })
                        .catch(function (e) {
                            ready[g] = false;
                            if (!Module["_tiledLoggedFirstErr"]) {
                                Module["_tiledLoggedFirstErr"] = 1;
                                console.warn("[TiledMap] first tile load failed: " + u + " — " + e.message);
                            }
                        });
                } else {
                    // Bitmap path: read the bytes verbatim and wrap
                    // them in a Blob URL. The old text-based path
                    // corrupted binary data (utf-8 decode) and stamped
                    // the wrong svg+xml MIME on the data URL, so PNG/
                    // JPG tiles silently failed to decode.
                    readAssetBinary(mu, u)
                        .then(function (bytes) {
                            const blob = new Blob([bytes], { "type": mt });
                            im.src = URL.createObjectURL(blob);
                        })
                        .catch(function (e) {
                            ready[g] = false;
                            if (!Module["_tiledLoggedFirstErr"]) {
                                Module["_tiledLoggedFirstErr"] = 1;
                                console.warn("[TiledMap] first tile load failed: " + u + " — " + e.message);
                            }
                        });
                }
            })(gid, img, url, memUrl, mime);
        }
        // tileSize is informational; drawing uses the MAP's tilewidth/
        // tileheight, not the tileset's. With multiple tilesets the last
        // one wins here — fine because nothing in the draw path reads it.
        M["tileSize"] = (ts["tilewidth"] | 0) || 256;
        console.log("[TiledMap] tileset loaded (firstgid " + firstgid + "): " + tilesArr.length + " tiles");
    }

    async function decompressTileLayer(layer) {
        const raw = layer["data"];
        if (typeof raw !== "string") return new Uint32Array(raw || []);
        const bin = atob(raw);
        const u8 = new Uint8Array(bin.length);
        for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
        const comp = layer["compression"];
        if (comp === "gzip" || comp === "zlib" || comp === "deflate") {
            if (typeof DecompressionStream === "undefined") {
                console.warn("[TiledMap] DecompressionStream unavailable; skipping " + layer["name"]);
                return new Uint32Array(0);
            }
            const fmt = comp === "zlib" ? "deflate" : comp === "gzip" ? "gzip" : "deflate-raw";
            const ds = new DecompressionStream(fmt);
            const buf = await new Response(new Blob([u8]).stream().pipeThrough(ds)).arrayBuffer();
            return new Uint32Array(buf);
        }
        return new Uint32Array(u8.buffer, u8.byteOffset, (u8.byteLength / 4) | 0);
    }

    async function load(mapPath) {
        const loadId = (M["loadId"] | 0) + 1;
        M["loadId"] = loadId;
        M["ready"] = false;
        M["layers"] = [];
        M["tileImages"] = {};
        M["objects"] = [];
        M["collisionRects"] = [];
        M["collisionPolys"] = [];
        M["warps"] = [];
        M["mapPath"] = mapPath;
        // Reset cached minimap bitmap so the next draw rebakes it.
        M["minimapBitmap"] = null;
        M["minimapKey"] = "";
        let mapJson;
        try {
            const txt = await readAssetText("/" + mapPath, mapPath);
            mapJson = JSON.parse(txt);
        } catch (e) {
            console.warn("[TiledMap] could not load " + mapPath + ": " + e.message);
            return;
        }
        if (M["loadId"] !== loadId) return;
        const mapLayers = mapJson["layers"] || [];
        const mapTilesets = mapJson["tilesets"] || [];
        console.log("[TiledMap] map fetched: "
            + mapLayers.length + " layers, "
            + mapTilesets.length + " tilesets, size "
            + (mapJson["width"] | 0) + "x" + (mapJson["height"] | 0));
        M["mapWidth"] = mapJson["width"] | 0;
        M["mapHeight"] = mapJson["height"] | 0;
        M["tileWidth"] = mapJson["tilewidth"] | 0;
        M["tileHeight"] = mapJson["tileheight"] | 0;

        try {
            await loadTilesets(mapJson, mapPath);
        } catch (e) {
            console.warn("[TiledMap] tileset load failed: " + e.message);
        }
        if (M["loadId"] !== loadId) return;

        for (let i = 0; i < mapLayers.length; i++) {
            if (M["loadId"] !== loadId) return;
            const layer = mapLayers[i];
            const lt = layer["type"];
            if (lt === "tilelayer") {
                try {
                    const data = await decompressTileLayer(layer);
                    const ln = layer["name"];
                    const lw = layer["width"] | 0;
                    const lh = layer["height"] | 0;
                    M["layers"].push({ name: ln, width: lw, height: lh, data: data });
                    // Mirror the server's solid-tile-layer collision
                    // derivation so the debug overlay shows every wall.
                    // Keep this set in sync with Server/TiledMap.cc.
                    if (ln === "cliff" || ln === "water" || ln === "dirt" || ln === "castle" || ln === "bush") {
                        const tw_ = M["tileWidth"] | 0;
                        const th_ = M["tileHeight"] | 0;
                        const tileShapes = M["tileShapes"] || {};
                        const tileImgSize = M["tileImageSize"] || {};
                        M["collisionRects"] = M["collisionRects"] || [];
                        M["collisionPolys"] = M["collisionPolys"] || [];
                        const FLIP_H_ = 0x80000000;
                        const FLIP_V_ = 0x40000000;
                        const FLIP_D_ = 0x20000000;
                        const GID_MASK_ = 0x1fffffff;
                        // Matches Server/TiledMap.cc::apply_flip — Tiled's
                        // CellRenderer flip composition with D as a 90°
                        // rotation about the tile centre, then H/V mirrors.
                        function flipPt(u, v, iw, ih, fH, fV, fD) {
                            if (fD) {
                                const sx = fV ? -1 : 1;
                                const sy = fH ? 1 : -1;
                                return {
                                    x: 0.5 * iw + sy * (0.5 * iw - v),
                                    y: 0.5 * ih + sx * (u - 0.5 * ih)
                                };
                            }
                            return {
                                x: fH ? (iw - u) : u,
                                y: fV ? (ih - v) : v
                            };
                        }
                        for (let r = 0; r < lh; r++) {
                            for (let cc = 0; cc < lw; cc++) {
                                const rawg = data[r * lw + cc];
                                if (!rawg) continue;
                                const gid = rawg & GID_MASK_;
                                if (!gid) continue;
                                const fH = (rawg & FLIP_H_) !== 0;
                                const fV = (rawg & FLIP_V_) !== 0;
                                const fD = (rawg & FLIP_D_) !== 0;
                                const shapes = tileShapes[gid];
                                const ox = cc * tw_;
                                const oy = r * th_;
                                if (!shapes || !shapes.length) {
                                    // No per-tile shape authored — treat
                                    // the whole tile as solid (matches
                                    // the old fallback behaviour).
                                    M["collisionRects"].push({
                                        x: ox, y: oy, w: tw_, h: th_
                                    });
                                    continue;
                                }
                                const sz = tileImgSize[gid];
                                const iw = (sz && sz.w > 0) ? sz.w : tw_;
                                const ih = (sz && sz.h > 0) ? sz.h : th_;
                                const sxw = tw_ / iw;
                                const syw = th_ / ih;
                                for (let si = 0; si < shapes.length; si++) {
                                    const sh = shapes[si];
                                    const out = [];
                                    let minx = Infinity, miny = Infinity;
                                    let maxx = -Infinity, maxy = -Infinity;
                                    for (let pi = 0; pi < sh.verts.length; pi++) {
                                        const p = sh.verts[pi];
                                        const fp = flipPt(p.x, p.y, iw, ih, fH, fV, fD);
                                        const wx = ox + fp.x * sxw;
                                        const wy = oy + fp.y * syw;
                                        out.push({ x: wx, y: wy });
                                        if (wx < minx) minx = wx;
                                        if (wy < miny) miny = wy;
                                        if (wx > maxx) maxx = wx;
                                        if (wy > maxy) maxy = wy;
                                    }
                                    M["collisionPolys"].push({
                                        verts: out,
                                        minx: minx, miny: miny,
                                        maxx: maxx, maxy: maxy
                                    });
                                }
                            }
                        }
                    }
                } catch (e) {
                    console.warn("[TiledMap] layer '" + layer["name"] + "' decode failed: " + e.message);
                }
            } else if (lt === "objectgroup" && layer["name"] === "img") {
                const objs = layer["objects"] || [];
                for (let j = 0; j < objs.length; j++) {
                    const o = objs[j];
                    const ogid = o["gid"];
                    if (!ogid) continue;
                    // Tiled positions tile-objects from their bottom-left.
                    const oh = +o["height"];
                    M["objects"].push({
                        gid: ogid | 0,
                        x: +o["x"],
                        y: (+o["y"]) - oh,
                        w: +o["width"],
                        h: oh
                    });
                }
            } else if (lt === "objectgroup" && layer["name"] === "collision") {
                // Mirror the server-side collision rect list so we can
                // overlay walls for debugging. Only the explicit rects
                // are captured here — the server *also* derives walls
                // from solid tile layers (cliff), so the in-game wall
                // set is a superset of this list.
                //
                // Mirror Server/TiledMap.cc::object_kind: newer Tiled
                // (1.10+) emits `class`; older versions emit `type`.
                // Maps in this repo mix both — fields3 uses `class`.
                const cobjs = layer["objects"] || [];
                M["collisionRects"] = M["collisionRects"] || [];
                for (let j = 0; j < cobjs.length; j++) {
                    const o = cobjs[j];
                    const kind = (o["type"] && o["type"].length) ? o["type"] : (o["class"] || "");
                    if (kind !== "collision") continue;
                    M["collisionRects"].push({
                        x: +o["x"], y: +o["y"], w: +o["width"], h: +o["height"]
                    });
                }
            } else if (lt === "objectgroup" && (layer["name"] === "warps" || layer["name"] === "checkpoints")) {
                // Mirror Server/TiledMap.cc::parse_warps_layer — only
                // objects of kind "warp" are portals. The server scans
                // both `warps` and `checkpoints` layers for warp
                // destinations (see read_warp_points_from_map at
                // TiledMap.cc:1091); some maps author the outbound warps
                // under `checkpoints` too, so we apply the same pair of
                // layer names here. Radius matches server:
                // max(96, max(w, h) * 0.5) so point-warps still have a
                // visible footprint. See `object_kind` above for the
                // type-vs-class detection.
                const wobjs = layer["objects"] || [];
                M["warps"] = M["warps"] || [];
                for (let j = 0; j < wobjs.length; j++) {
                    const o = wobjs[j];
                    const kind = (o["type"] && o["type"].length) ? o["type"] : (o["class"] || "");
                    if (kind !== "warp") continue;
                    const ww = +o["width"] || 0;
                    const wh = +o["height"] || 0;
                    M["warps"].push({
                        x: +o["x"], y: +o["y"],
                        radius: Math.max(96, Math.max(ww, wh) * 0.5),
                        name: o["name"] || ""
                    });
                }
            }
        }
        if (M["mapPath"] !== mapPath) return;
        M["ready"] = true;
        console.log("[TiledMap] ready: " + M["layers"].length + " tile layers, "
            + Object.keys(M["tileImages"]).length + " tile images, "
            + M["objects"].length + " image objects");
    }

    Module["tiledMapLoad"] = load;
    load("Map/main/main.tmj");
});

EM_JS(void, tiled_map_set_map_js, (char const *path_ptr), {
    const mapPath = UTF8ToString(path_ptr);
    if (!Module["tiledMap"] || !Module["tiledMapLoad"]) return;
    if (Module["tiledMap"]["mapPath"] === mapPath) return;
    Module["tiledMapLoad"](mapPath);
});

EM_JS(void, tiled_map_draw_js, (int ctx_id), {
    const M = Module["tiledMap"];
    if (!M || !M["ready"]) return;
    const c = Module.ctxs[ctx_id];
    if (!c) {
        if (!Module["_tiledLoggedNoCtx"]) {
            Module["_tiledLoggedNoCtx"] = 1;
            console.warn("[TiledMap] draw: no canvas context for id " + ctx_id);
        }
        return;
    }
    const tw = M["tileWidth"] | 0;
    const th = M["tileHeight"] | 0;
    if (tw <= 0 || th <= 0) {
        if (!Module["_tiledLoggedNoTile"]) {
            Module["_tiledLoggedNoTile"] = 1;
            console.warn("[TiledMap] draw: tile size invalid tw=" + tw + " th=" + th);
        }
        return;
    }

    // Recover the visible world rectangle from the current 2D transform —
    // the C++ Renderer last set this with the camera matrix. Mapping the
    // four canvas corners back through the inverse gives us the viewport
    // in world coords so we can cull off-screen tiles.
    const t = c.getTransform();
    const cw = c.canvas.width;
    const ch = c.canvas.height;
    const det = t.a * t.d - t.b * t.c;
    if (!det) return;
    const ia = t.d / det;
    const ib = -t.b / det;
    const ic = -t.c / det;
    const id = t.a / det;
    const ie = -(ia * t.e + ic * t.f);
    const ifv = -(ib * t.e + id * t.f);

    let lx = Infinity;
    let topY = Infinity;
    let rx = -Infinity;
    let by = -Infinity;
    const corners = [[0, 0], [cw, 0], [cw, ch], [0, ch]];
    for (let i = 0; i < corners.length; i++) {
        const px = corners[i][0];
        const py = corners[i][1];
        const wx = ia * px + ic * py + ie;
        const wy = ib * px + id * py + ifv;
        if (wx < lx) lx = wx;
        if (wx > rx) rx = wx;
        if (wy < topY) topY = wy;
        if (wy > by) by = wy;
    }

    // Tiled stores horizontal/vertical/diagonal flip bits in the top three
    // bits of the gid. The map authors use these heavily (40%+ of tiles in
    // some layers) so we have to honour them; otherwise the recognisable
    // shape comes through but every individual tile looks wrong.
    const GID_MASK = 0x1fffffff;
    const FLIP_H = 0x80000000;
    const FLIP_V = 0x40000000;
    const FLIP_D = 0x20000000;
    const layers = M["layers"];
    const tileImages = M["tileImages"];
    const tileReady = M["tileReady"] || {};

    let dbgDrawn = 0;
    let dbgSkipped = 0;
    let dbgNonZero = 0;
    let dbgErrored = 0;

    for (let li = 0; li < layers.length; li++) {
        const layer = layers[li];
        const lw = layer.width;
        const sx = Math.max(0, Math.floor(lx / tw));
        const sy = Math.max(0, Math.floor(topY / th));
        const ex = Math.min(lw, Math.ceil(rx / tw));
        const ey = Math.min(layer.height, Math.ceil(by / th));
        const data = layer.data;
        for (let row = sy; row < ey; row++) {
            const rowOff = row * lw;
            const drawY = row * th;
            for (let col = sx; col < ex; col++) {
                const rawg = data[rowOff + col];
                if (!rawg) continue;
                dbgNonZero++;
                const gid = rawg & GID_MASK;
                if (!gid) continue;
                const img = tileImages[gid];
                if (!img || tileReady[gid] !== true) { dbgSkipped++; continue; }
                const fH = (rawg & FLIP_H) !== 0;
                const fV = (rawg & FLIP_V) !== 0;
                const fD = (rawg & FLIP_D) !== 0;
                try {
                    if (!fH && !fV && !fD) {
                        c.drawImage(img, col * tw, drawY, tw, th);
                    } else {
                        // Pivot at tile center, apply Tiled's flip
                        // composition, then draw at (-tw/2,-th/2).
                        // Order matters: diagonal flip is a transpose
                        // (swap X/Y axes); H and V are then mirrors of
                        // the resulting orientation.
                        c.save();
                        c.translate(col * tw + tw * 0.5, drawY + th * 0.5);
                        // Matches Tiled's CellRenderer::render in
                        // src/libtiled/maprenderer.cpp. When D is set,
                        // Tiled rotates 90° CW and re-derives H/V from
                        // the original H/V bits:
                        //   newH = origV
                        //   newV = !origH
                        // Then applies scaleX = newH ? -1 : 1 etc.
                        //
                        // Visual table (square tiles):
                        //   D       → main-diagonal flip (TR↔BL)
                        //   D+H     → rotate 90° CW
                        //   D+V     → rotate 90° CCW
                        //   D+H+V   → anti-diagonal flip (TL↔BR)
                        //
                        // My earlier "spec canonical" mapping had D-only
                        // and D+H+V swapped — Tiled's actual code is the
                        // authority here.
                        let nH = fH, nV = fV;
                        if (fD) {
                            c.rotate(Math.PI * 0.5);
                            nH = fV;
                            nV = !fH;
                        }
                        if (nH) c.scale(-1, 1);
                        if (nV) c.scale(1, -1);
                        c.drawImage(img, -tw * 0.5, -th * 0.5, tw, th);
                        c.restore();
                    }
                    dbgDrawn++;
                } catch (e) {
                    // A previously-loaded Image can land in the "broken"
                    // state if its decode fails later (rare). Mark it
                    // unready so future frames skip it.
                    tileReady[gid] = false;
                    dbgErrored++;
                }
            }
        }
    }
    {
        // Log on the first draw, when we first paint any tile, and every
        // ~120 frames thereafter — enough to see if images eventually
        // load without spamming the console.
        const prevDrawn = Module["_tiledDrawnEver"] | 0;
        const frame = (Module["_tiledFrame"] | 0) + 1;
        Module["_tiledFrame"] = frame;
        const totalImages = Object.keys(tileImages).length;
        let loadedImages = 0;
        let erroredImages = 0;
        let inflightImages = 0;
        let sampleErrSrc = "";
        let sampleOkSrc = "";
        for (const k in tileImages) {
            if (tileReady[k] === true) {
                loadedImages++;
                if (!sampleOkSrc) sampleOkSrc = tileImages[k].src;
            } else if (tileReady[k] === false) {
                erroredImages++;
                if (!sampleErrSrc) sampleErrSrc = tileImages[k].src;
            } else {
                inflightImages++;
            }
        }
        const firstPaint = dbgDrawn > 0 && prevDrawn === 0;
        if (firstPaint) Module["_tiledDrawnEver"] = 1;
        const shouldLog = !Module["_tiledFirstDraw"]
            || firstPaint
            || (frame % 120) === 0;
        if (shouldLog) {
            Module["_tiledFirstDraw"] = 1;
            console.log("[TiledMap] draw frame " + frame
                + ": tw=" + tw + " th=" + th
                + " transform a=" + t.a.toFixed(3) + " e=" + t.e.toFixed(0) + " f=" + t.f.toFixed(0)
                + " canvas=" + cw + "x" + ch
                + " viewport(world)=[" + lx.toFixed(0) + "," + topY.toFixed(0)
                + " -> " + rx.toFixed(0) + "," + by.toFixed(0) + "]"
                + " layers=" + layers.length
                + " nonZero=" + dbgNonZero + " drawn=" + dbgDrawn + " skipped=" + dbgSkipped
                + " drawErr=" + dbgErrored
                + " imgs=" + loadedImages + "ok/" + erroredImages + "err/" + inflightImages + "loading/" + totalImages + "total"
                + (sampleErrSrc ? " sampleErr=" + sampleErrSrc : "")
                + (sampleOkSrc ? " sampleOk=" + sampleOkSrc : ""));
        }
    }

    // Debug overlay: filename + flags on each visible tile, plus
    // translucent red over collision rects. Toggle via
    // `Module._tiledDebug = false` in the console.
    if (Module["_tiledDebug"] !== false) {
        const names = M["tileNames"] || {};
        c.save();
        c.fillStyle = "white";
        c.strokeStyle = "black";
        c.lineWidth = 3;
        c.font = "32px monospace";
        c.textAlign = "left";
        c.textBaseline = "top";
        for (let li = 0; li < layers.length; li++) {
            const layer = layers[li];
            const lw = layer.width;
            const sx = Math.max(0, Math.floor(lx / tw));
            const sy = Math.max(0, Math.floor(topY / th));
            const ex = Math.min(lw, Math.ceil(rx / tw));
            const ey = Math.min(layer.height, Math.ceil(by / th));
            const data = layer.data;
            for (let row = sy; row < ey; row++) {
                for (let col = sx; col < ex; col++) {
                    const rawg = data[row * lw + col];
                    if (!rawg) continue;
                    const gid = rawg & GID_MASK;
                    if (!gid) continue;
                    const nm = names[gid] || ("gid" + gid);
                    const fH = (rawg & FLIP_H) !== 0 ? "H" : "";
                    const fV = (rawg & FLIP_V) !== 0 ? "V" : "";
                    const fD = (rawg & FLIP_D) !== 0 ? "D" : "";
                    const flagStr = (fH + fV + fD) ? " " + fH + fV + fD : "";
                    const txt = layer.name + ":" + nm + flagStr;
                    const x = col * tw + 8;
                    const y = row * th + 8 + li * 36;
                    c.strokeText(txt, x, y);
                    c.fillText(txt, x, y);
                }
            }
        }
        c.restore();
    }

    // Debug overlay: translucent red over every collision rect
    // (explicit `collision` objectgroup + full-tile fallbacks) and
    // every per-tile collision polygon. Toggle via
    // `Module._tiledDebug = false` in the console to hide.
    if (Module["_tiledDebug"] !== false) {
        const rects = M["collisionRects"] || [];
        c.save();
        c.fillStyle = "rgba(255,0,0,0.25)";
        c.strokeStyle = "rgba(255,0,0,0.7)";
        c.lineWidth = 4;
        for (let i = 0; i < rects.length; i++) {
            const r = rects[i];
            if (r.x + r.w < lx || r.x > rx || r.y + r.h < topY || r.y > by) continue;
            c.fillRect(r.x, r.y, r.w, r.h);
            c.strokeRect(r.x, r.y, r.w, r.h);
        }
        const polys = M["collisionPolys"] || [];
        for (let i = 0; i < polys.length; i++) {
            const p = polys[i];
            if (p.maxx < lx || p.minx > rx || p.maxy < topY || p.miny > by) continue;
            const v = p.verts;
            if (!v || v.length < 3) continue;
            c.beginPath();
            c.moveTo(v[0].x, v[0].y);
            for (let j = 1; j < v.length; j++) c.lineTo(v[j].x, v[j].y);
            c.closePath();
            c.fill();
            c.stroke();
        }
        c.restore();
    }

    const objects = M["objects"];
    for (let oi = 0; oi < objects.length; oi++) {
        const o = objects[oi];
        if (o.x + o.w < lx) continue;
        if (o.x > rx) continue;
        if (o.y + o.h < topY) continue;
        if (o.y > by) continue;
        const ogid2 = o.gid & GID_MASK;
        const img = tileImages[ogid2];
        if (!img || tileReady[ogid2] !== true) continue;
        try { c.drawImage(img, o.x, o.y, o.w, o.h); }
        catch (e) { tileReady[ogid2] = false; }
    }

    // Portals: flat green disc with a darker rim, matching the game's
    // flower/petal aesthetic (flat fill + HSV-darkened stroke). Static
    // shape, no pulse — the colour is what reads as "interactable".
    const warps = M["warps"] || [];
    if (warps.length) {
        c.save();
        for (let wi = 0; wi < warps.length; wi++) {
            const w = warps[wi];
            if (w.x + w.radius < lx || w.x - w.radius > rx) continue;
            if (w.y + w.radius < topY || w.y - w.radius > by) continue;
            const r = w.radius;
            c.lineWidth = Math.max(4, r * 0.15);
            c.lineJoin = "round";
            c.fillStyle = "#3fc26b";
            c.strokeStyle = "#2a8c4d";
            c.beginPath();
            c.arc(w.x, w.y, r, 0, Math.PI * 2);
            c.fill();
            c.stroke();
        }
        c.restore();
    }
});

// Minimap-side draw. Caller has set up the transform so world coords
// land inside the minimap rect. We bake the entire minimap (white
// background, black collision rects, green portal dots) into an
// offscreen canvas once per map and then draw that bitmap — drawing
// hundreds of small rects through the world->minimap transform every
// frame produces fuzzy anti-aliased edges at fractional pixel offsets;
// pre-rasterising at a fixed high resolution sidesteps that and lets
// the browser do a single clean downsample to the minimap.
//
// Zones are intentionally NOT drawn — when a tiled map is present, the
// map silhouette is the more accurate guide.
EM_JS(void, tiled_map_draw_minimap_js, (int ctx_id, float arena_w, float arena_h), {
    const M = Module["tiledMap"];
    if (!M || !M["ready"]) return;
    const c = Module.ctxs[ctx_id];
    if (!c) return;
    if (!(arena_w > 0) || !(arena_h > 0)) return;

    const cacheKey = M["mapPath"] + "|" + arena_w + "x" + arena_h;
    let bmp = M["minimapBitmap"];
    if (!bmp || M["minimapKey"] !== cacheKey) {
        // Bake at a fixed pixel budget on the long edge so the bitmap is
        // crisp regardless of map size. 1024 is well above any reasonable
        // minimap display size, so downsampling stays clean on retina.
        const longEdge = 1024;
        const ratio = arena_w >= arena_h ? 1 : arena_h / arena_w;
        const bmpW = arena_w >= arena_h ? longEdge : Math.round(longEdge * (arena_w / arena_h));
        const bmpH = arena_w >= arena_h ? Math.round(longEdge * (arena_h / arena_w)) : longEdge;
        const off = (typeof OffscreenCanvas !== "undefined")
            ? new OffscreenCanvas(bmpW, bmpH)
            : (function () { const cv = document.createElement("canvas"); cv.width = bmpW; cv.height = bmpH; return cv; })();
        const oc = off.getContext("2d");
        // World -> bitmap-pixel scale. fillRect at integer-ish bitmap
        // coords stays edge-aligned because the map's tile grid is a
        // simple divisor of arena_w/arena_h.
        const sx = bmpW / arena_w;
        const sy = bmpH / arena_h;
        oc.setTransform(sx, 0, 0, sy, 0, 0);
        oc.fillStyle = "#ffffff";
        oc.fillRect(0, 0, arena_w, arena_h);
        oc.fillStyle = "#000000";
        const rects = M["collisionRects"] || [];
        for (let i = 0; i < rects.length; i++) {
            const r = rects[i];
            oc.fillRect(r.x, r.y, r.w, r.h);
        }
        const polys = M["collisionPolys"] || [];
        for (let i = 0; i < polys.length; i++) {
            const p = polys[i];
            const v = p.verts;
            if (!v || v.length < 3) continue;
            oc.beginPath();
            oc.moveTo(v[0].x, v[0].y);
            for (let j = 1; j < v.length; j++) oc.lineTo(v[j].x, v[j].y);
            oc.closePath();
            oc.fill();
        }
        const warps = M["warps"] || [];
        if (warps.length) {
            const dotR = Math.max(arena_w, arena_h) / 70;
            oc.fillStyle = "#3fc26b";
            oc.strokeStyle = "#2a8c4d";
            oc.lineWidth = Math.max(arena_w, arena_h) / 400;
            for (let i = 0; i < warps.length; i++) {
                const w = warps[i];
                oc.beginPath();
                oc.arc(w.x, w.y, dotR, 0, Math.PI * 2);
                oc.fill();
                oc.stroke();
            }
        }
        M["minimapBitmap"] = off;
        M["minimapKey"] = cacheKey;
        bmp = off;
    }
    // Browser smoothing handles the downsample to the actual minimap
    // size — much cleaner than the raw transformed fillRect path.
    c.save();
    c.imageSmoothingEnabled = true;
    c.imageSmoothingQuality = "high";
    c.drawImage(bmp, 0, 0, arena_w, arena_h);
    c.restore();
});

EM_JS(int, tiled_map_is_ready_js, (), {
    return (Module["tiledMap"] && Module["tiledMap"]["ready"]) ? 1 : 0;
});

namespace TiledMapRender {

void init() { 
    #if DEBUG
    EM_ASM({
        Module._tiledDebug = true;
    });
    #else
    EM_ASM({
        Module._tiledDebug = false;
    });
    #endif
    tiled_map_init_js();
}

void set_map(std::string const &map_path) { tiled_map_set_map_js(map_path.c_str()); }

void draw(Renderer &ctx) { tiled_map_draw_js(ctx.id); }

void draw_minimap(Renderer &ctx, float arena_w, float arena_h) {
    tiled_map_draw_minimap_js(ctx.id, arena_w, arena_h);
}

bool is_ready() { return tiled_map_is_ready_js() != 0; }

} // namespace TiledMapRender
