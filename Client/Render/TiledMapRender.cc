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
    Module["tiledMap"] = { "ready": false, "layers": [], "tileImages": {}, "objects": [] };

    const M = Module["tiledMap"];

    async function loadTileset(mapJson, mapPath) {
        const tsList = mapJson["tilesets"];
        const tsRef = tsList && tsList[0];
        if (!tsRef) { console.warn("[TiledMap] map has no tilesets"); return; }
        const firstgid = (tsRef["firstgid"] | 0);
        const mapDir = mapPath.substring(0, mapPath.lastIndexOf("/"));
        const baseHref = location.href.replace(/[^/]*$/, "");
        const tsUrl = new URL(tsRef["source"], baseHref + mapDir + "/");
        const tsPath = tsUrl.pathname;
        console.log("[TiledMap] fetching tileset " + tsPath);
        const r = await fetch(tsPath);
        if (!r.ok) throw new Error("tileset http " + r.status + " for " + tsPath);
        const ts = await r.json();
        const tsDir = tsPath.substring(0, tsPath.lastIndexOf("/"));
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
        for (let i = 0; i < tilesArr.length; i++) {
            const t = tilesArr[i];
            const gid = firstgid + ((t["id"] | 0));
            const url = tsDir + "/" + t["image"];
            const img = new Image();
            M["tileImages"][gid] = img;
            names[gid] = t["image"];
            (function (g, im, u) {
                im.addEventListener("load", function () { ready[g] = true; });
                im.addEventListener("error", function () { ready[g] = false; });
                fetch(u)
                    .then(function (r) {
                        if (!r.ok) throw new Error("http " + r.status);
                        return r.text();
                    })
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
                            console.warn("[TiledMap] first tile fetch failed: " + u + " — " + e.message);
                        }
                    });
            })(gid, img, url);
        }
        M["tileSize"] = (ts["tilewidth"] | 0) || 256;
        console.log("[TiledMap] tileset loaded: " + tilesArr.length + " tiles");
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

    async function load() {
        const mapPath = "Map/main/main.tmj";
        let mapJson;
        try {
            const r = await fetch(mapPath);
            if (!r.ok) throw new Error("http " + r.status);
            mapJson = await r.json();
        } catch (e) {
            console.warn("[TiledMap] could not load " + mapPath + ": " + e.message);
            return;
        }
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
            await loadTileset(mapJson, mapPath);
        } catch (e) {
            console.warn("[TiledMap] tileset load failed: " + e.message);
        }

        for (let i = 0; i < mapLayers.length; i++) {
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
                        M["collisionRects"] = M["collisionRects"] || [];
                        for (let r = 0; r < lh; r++) {
                            for (let cc = 0; cc < lw; cc++) {
                                const v = data[r * lw + cc] & 0x1fffffff;
                                if (!v) continue;
                                M["collisionRects"].push({
                                    x: cc * tw_, y: r * th_, w: tw_, h: th_
                                });
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
                const cobjs = layer["objects"] || [];
                M["collisionRects"] = M["collisionRects"] || [];
                for (let j = 0; j < cobjs.length; j++) {
                    const o = cobjs[j];
                    if (o["type"] !== "collision") continue;
                    M["collisionRects"].push({
                        x: +o["x"], y: +o["y"], w: +o["width"], h: +o["height"]
                    });
                }
            }
        }
        M["ready"] = true;
        console.log("[TiledMap] ready: " + M["layers"].length + " tile layers, "
            + Object.keys(M["tileImages"]).length + " tile images, "
            + M["objects"].length + " image objects");
    }

    load();
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

    // Debug overlay: translucent red boxes over every collision rect
    // (explicit `collision` objectgroup + derived from solid tile layers).
    // Toggle via `Module._tiledDebug = false` in the console to hide.
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

void draw(Renderer &ctx) { tiled_map_draw_js(ctx.id); }

bool is_ready() { return tiled_map_is_ready_js() != 0; }

} // namespace TiledMapRender
