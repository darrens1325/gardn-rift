#pragma once

class Renderer;

#include <string>

namespace TiledMapRender {
    // Kicks off the async fetch + parse + image preload. Safe to call once
    // at startup; subsequent calls are no-ops.
    void init();

    void set_map(std::string const &map_path);

    // Draws the map's tile layers (and the "img" object layer) into the
    // current world transform. No-op until the loader has populated the
    // tile image cache.
    void draw(Renderer &ctx);

    // True once the map JSON, tileset, and tile-layer decompression are
    // all complete. Image rasterization is still asynchronous after this
    // returns true — `draw` skips any tile whose Image isn't yet decoded.
    bool is_ready();
}
