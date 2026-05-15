# Bundle build: client + server + bots + assets in a single .wasm.
#
# Differences from the standalone Client/Server CMakeLists:
#   * Wraps every .cc from Client/ Server/ Shared/ in a per-side C++
#     namespace (gardn::client / gardn::server) via Bundle/wrap_sources.py.
#     Same Shared/*.cc is compiled twice with different macros so the
#     CLIENTSIDE/SERVERSIDE Float-vs-LerpFloat split doesn't leak across.
#   * Drops Server/Native.cc (uWebSockets) and the WASM-server's HTTP
#     harness; replaces them with Bundle/Bridge.cc which exposes a stable
#     C ABI: client_*, server_*, bot_*.
#   * Embeds Map/main/main.tmj and tiles/ via --embed-file so the server's
#     TiledMap.cc std::ifstream paths work unchanged against MEMFS.
#   * Builds as a SINGLE_FILE Emscripten output so the resulting .wasm is
#     base64'd into the .js loader and the deliverable is one file.
#
# Build:
#   cd Bundle && mkdir build && cd build
#   cmake ..
#   make
#   # Output: build/gardn-bundle.js  (use Bundle/index.html as the host page)
#
# Optional flags:
#   -DDEBUG=1            assertions + dwarf debug info, no closure
#   -DTPS=N              server tick rate (default 20)
#   -DBUNDLE_BOTS=N      number of bots to spawn at startup (default 0 — bots
#                        are driven from JS; this just hints the harness)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(_em_root "")
    if (DEFINED ENV{EMSDK})
        set(_em_root "$ENV{EMSDK}/upstream/emscripten")
    endif()
    if (NOT _em_root OR NOT EXISTS "${_em_root}/cmake/Modules/Platform/Emscripten.cmake")
        execute_process(COMMAND em-config EMSCRIPTEN_ROOT
                        OUTPUT_VARIABLE _em_root
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _em_res
                        ERROR_QUIET)
        if (NOT _em_res EQUAL 0)
            set(_em_root "")
        endif()
    endif()
    if (_em_root AND EXISTS "${_em_root}/cmake/Modules/Platform/Emscripten.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${_em_root}/cmake/Modules/Platform/Emscripten.cmake"
            CACHE FILEPATH "Emscripten toolchain (auto-detected)" FORCE)
        message(STATUS "gardn-bundle auto-loaded Emscripten toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(FATAL_ERROR
            "gardn-bundle requires the Emscripten toolchain. Set EMSDK or run "
            "via `emcmake cmake ..`.")
    endif()
endif()

set(CMAKE_SYSTEM_NAME Generic)
cmake_minimum_required(VERSION 3.16)
project(gardn-bundle)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(WRAP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/wrap_sources.py")
set(WRAP_DIR "${CMAKE_CURRENT_BINARY_DIR}/wrapped")
set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

include_directories(${REPO_ROOT})

# Derive ARENA_WIDTH / ARENA_HEIGHT from Map/main/main.tmj at configure
# time. Writes Shared/MapDimensions.hh into the build tree; the include
# path below makes <Shared/MapDimensions.hh> resolve to it.
include(${REPO_ROOT}/Shared/MapDims.cmake)
gardn_derive_arena_dims("${REPO_ROOT}/Map/main/main.tmj")
include_directories(${GARDN_MAP_DIMS_INCLUDE_DIR})

# --- Client-side wrapped sources (CLIENTSIDE=1) -------------------------------
set(CLIENT_SRCS
    Client/Assets/Flower.cc
    Client/Assets/Mob.cc
    Client/Assets/Petal.cc
    Client/Assets/Web.cc
    Client/Render/RenderDrop.cc
    Client/Render/RenderFlower.cc
    Client/Render/RenderHealth.cc
    Client/Render/RenderMob.cc
    Client/Render/RenderName.cc
    Client/Render/RenderPetal.cc
    Client/Render/RenderWeb.cc
    Client/Render/Renderer.cc
    Client/Render/TiledMapRender.cc
    Client/Debug.cc
    Client/DOM.cc
    Client/Game.cc
    Client/Input.cc
    Client/Main.cc
    Client/Network.cc
    Client/Particle.cc
    Client/Rendering.cc
    Client/Setup.cc
    Client/Simulation.cc
    Client/Socket.cc
    Client/StaticData.cc
    Client/Storage.cc
    Client/Ui/InGame/InputFreeze.cc
    Client/Ui/InGame/Leaderboard.cc
    Client/Ui/InGame/LevelBar.cc
    Client/Ui/InGame/LoadoutPetal.cc
    Client/Ui/InGame/LoadoutSlot.cc
    Client/Ui/InGame/Map.cc
    Client/Ui/InGame/OverlevelTimer.cc
    Client/Ui/InGame/RoundEndBanner.cc
    Client/Ui/InGame/Tooltip.cc
    Client/Ui/TitleScreen/Changelog.cc
    Client/Ui/TitleScreen/DeathScreen.cc
    Client/Ui/TitleScreen/MainScreen.cc
    Client/Ui/TitleScreen/MobGallery.cc
    Client/Ui/TitleScreen/PetalGallery.cc
    Client/Ui/TitleScreen/Settings.cc
    Client/Ui/TitleScreen/StatScreen.cc
    Client/Ui/Button.cc
    Client/Ui/Choose.cc
    Client/Ui/Container.cc
    Client/Ui/DynamicText.cc
    Client/Ui/Element.cc
    Client/Ui/Extern.cc
    Client/Ui/ScrollContainer.cc
    Client/Ui/StaticText.cc
    Client/Ui/TextInput.cc
    Client/Ui/Window.cc
    Shared/Arena.cc
    Shared/Binary.cc
    Shared/Config.cc
    Shared/Entity.cc
    Shared/EntityDef.cc
    Shared/Helpers.cc
    Shared/Map.cc
    Shared/Simulation.cc
    Shared/StaticData.cc
    Shared/Vector.cc
)

# --- Server-side wrapped sources (SERVERSIDE=1) -------------------------------
set(SERVER_SRCS
    Server/EntityFunctions/Damage.cc
    Server/EntityFunctions/Death.cc
    Server/EntityFunctions/Detection.cc
    Server/EntityFunctions/Misc.cc
    Server/Process/Ai.cc
    Server/Process/Camera.cc
    Server/Process/Collision.cc
    Server/Process/Culling.cc
    Server/Process/Flower.cc
    Server/Process/Health.cc
    Server/Process/Motion.cc
    Server/Process/Petal.cc
    Server/Process/Score.cc
    Server/Process/Segment.cc
    Server/Client.cc
    Server/Game.cc
    Server/Main.cc
    Server/PetalTracker.cc
    Server/Server.cc
    Server/Simulation.cc
    Server/Spawn.cc
    Server/SpatialHashUniform.cc
    Server/TeamManager.cc
    Server/TiledMap.cc
    Server/Wasm.cc
    Shared/Arena.cc
    Shared/Binary.cc
    Shared/Config.cc
    Shared/Entity.cc
    Shared/EntityDef.cc
    Shared/Helpers.cc
    Shared/Map.cc
    Shared/Simulation.cc
    Shared/StaticData.cc
    Shared/Vector.cc
    # The bot driver lives under Bundle/ but compiles as a SERVER-side TU
    # so it sees the server's Float=float layout.
    Bundle/BotDriver.cc
)

function(generate_wrapped_sources side src_var out_var)
    set(out_list "")
    foreach(src IN LISTS ${src_var})
        set(wrapped "${WRAP_DIR}/${side}/${src}.wrap.cc")
        get_filename_component(wrapped_dir "${wrapped}" DIRECTORY)
        add_custom_command(
            OUTPUT "${wrapped}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${wrapped_dir}"
            COMMAND ${Python3_EXECUTABLE} "${WRAP_SCRIPT}" "${side}" "${src}" "${wrapped}" "${REPO_ROOT}"
            DEPENDS "${REPO_ROOT}/${src}" "${WRAP_SCRIPT}" "${CMAKE_CURRENT_SOURCE_DIR}/Prologue.hh"
            COMMENT "Wrap ${src} (gardn::${side})"
            VERBATIM
        )
        list(APPEND out_list "${wrapped}")
    endforeach()
    set(${out_var} "${out_list}" PARENT_SCOPE)
endfunction()

generate_wrapped_sources(client CLIENT_SRCS CLIENT_WRAPPED)
generate_wrapped_sources(server SERVER_SRCS SERVER_WRAPPED)

# Separate static archives so each side compiles with its own -D flags.
add_library(gardn_client_obj OBJECT ${CLIENT_WRAPPED})
target_compile_options(gardn_client_obj PRIVATE
    -std=c++20
    -DCLIENTSIDE=1
    -DGARDN_BUNDLE=1
    # Emscripten requires -sUSE_ZLIB=1 at *compile* time for `#include <zlib.h>`
    # to resolve. The client doesn't itself use zlib, but Bundle/Prologue.hh
    # pulls it in unconditionally so the server's TiledMap.cc TU can find it
    # inside the namespace.
    -sUSE_ZLIB=1
)
add_library(gardn_server_obj OBJECT ${SERVER_WRAPPED})
target_compile_options(gardn_server_obj PRIVATE
    -std=c++20
    -DSERVERSIDE=1
    -DWASM_SERVER=1
    -DGARDN_BUNDLE=1
    -sUSE_ZLIB=1
)

if (DEBUG)
    target_compile_options(gardn_client_obj PRIVATE -DDEBUG=1 -gdwarf-4 -sNO_DISABLE_EXCEPTION_CATCHING)
    target_compile_options(gardn_server_obj PRIVATE -DDEBUG=1 -gdwarf-4)
else()
    target_compile_options(gardn_client_obj PRIVATE -O3 -ffast-math)
    target_compile_options(gardn_server_obj PRIVATE -O3 -ffast-math)
endif()
if (TPS)
    target_compile_options(gardn_client_obj PRIVATE -DGARDN_TPS=${TPS})
    target_compile_options(gardn_server_obj PRIVATE -DGARDN_TPS=${TPS})
endif()

# --- Bundled ONNX policy ------------------------------------------------------
#
# Bots/model.onnx (when present) is embedded INSIDE the wasm via --embed-file
# below. Two CMake variables — defined here, used twice — drive the staging
# pipeline:
#   STAGED_MODEL: the always-existing copy we hand to file_packager.
#   MODEL_PHONY:  a "touched-every-build" marker that forces stage_model.cmake
#                 to re-run on every `make` (so a new model produced after
#                 `cmake ..` still gets picked up).
# See the "Bundled ONNX policy — staging pipeline" block lower down for the
# actual rules.
set(STAGED_MODEL "${CMAKE_CURRENT_BINARY_DIR}/embed/model.onnx")
# Force STAGED_MODEL to exist before configure finishes — otherwise the
# linker's --embed-file resolution on the first build hits a missing file.
execute_process(COMMAND ${CMAKE_COMMAND}
                -DSRC=${REPO_ROOT}/Bots/model.onnx
                -DDST=${STAGED_MODEL}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/stage_model.cmake)

# Bridge.cc is global-scope (no namespace wrapper).
add_executable(gardn-bundle
    Bridge.cc
    $<TARGET_OBJECTS:gardn_client_obj>
    $<TARGET_OBJECTS:gardn_server_obj>
)
target_compile_options(gardn-bundle PRIVATE -std=c++20 -DGARDN_BUNDLE=1)

# C-ABI surface visible to JS. Each entry must be prefixed with `_`.
set(EXPORTED_FUNCS
    _main
    _bundle_main
    _client_on_message
    _client_key_event
    _client_mouse_event
    _client_wheel_event
    _client_loop
    _server_on_connect
    _server_on_disconnect
    _server_on_message
    _server_tick
    _server_incoming_buffer_ptr
    _server_incoming_buffer_cap
    _bot_make_obs
    _bot_apply_action
    _bot_obs_dim
    _bot_num_actions
    _bot_alive_count
    _malloc
    _free
)
string(REPLACE ";" "," EXPORTED_FUNCS_CSV "${EXPORTED_FUNCS}")

target_link_options(gardn-bundle PRIVATE
    -sEXIT_RUNTIME=0
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=64MB
    -sMODULARIZE=1
    -sEXPORT_NAME=GardnBundle
    -sSINGLE_FILE=1
    -g
    # Force base64-encoded inline data instead of Emscripten's default
    # "bytes-as-latin1-chars" trick, which produces a binary-looking .js
    # that doesn't survive UTF-8-only servers / tooling.
    -sSINGLE_FILE_BINARY_ENCODE=1
    # FS is exported so the harness can pull embedded files (Map/, tiles/)
    # out of MEMFS and serve them in JS-level fetch() calls (the client's
    # TiledMapRender uses fetch, not std::ifstream).
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAPF32,ccall,cwrap,UTF8ToString,FS
    -sEXPORTED_FUNCTIONS=${EXPORTED_FUNCS_CSV}
    -sUSE_ZLIB=1
    # Emscripten's --embed-file takes a single argv "src@dst" token. CMake
    # joins target_link_options entries with spaces unless we pass each
    # multi-word flag as two separate list items via SHELL: (which avoids
    # de-duplication / quoting) — but the simplest form that survives
    # generators is to give them as two distinct tokens.
    "SHELL:--embed-file ${REPO_ROOT}/Map/main/main.tmj@/Map/main/main.tmj"
    "SHELL:--embed-file ${REPO_ROOT}/tiles@/tiles"
    # The trained DQN policy is bundled INTO the wasm — index.html reads
    # it from MEMFS at boot and hands the bytes to onnxruntime-web. When
    # no policy is exported, the staged file is empty (0 bytes) and the
    # bots fall back to uniform-random movement actions.
    "SHELL:--embed-file ${STAGED_MODEL}@/model.onnx"
)
# Re-link the bundle whenever the embedded model bytes change. stage_model
# only bumps STAGED_MODEL's mtime on actual content change, so unchanged
# builds skip the link.
set_property(TARGET gardn-bundle APPEND PROPERTY LINK_DEPENDS "${STAGED_MODEL}")
add_dependencies(gardn-bundle gardn-bundle-stage-model)
target_link_libraries(gardn-bundle PRIVATE)

if (NOT DEBUG)
    target_link_options(gardn-bundle PRIVATE --closure=0)
endif()

set_target_properties(gardn-bundle PROPERTIES SUFFIX ".js")

# Drop Bundle/index.html next to the bundle so a plain `python -m http.server`
# in build/ Just Works. We re-copy on every build (not via configure_file) so
# edits to Bundle/index.html flow without needing a re-cmake.
#
# This uses a file-level rule (output: build/index.html, depends: Bundle/index.html)
# so CMake re-runs the copy whenever the source HTML changes — even when the
# wasm itself didn't relink. A POST_BUILD command on gardn-bundle would only
# fire when the target itself rebuilt, which silently skips HTML-only edits.
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
            "${CMAKE_CURRENT_BINARY_DIR}/index.html"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
    COMMENT "Stage Bundle/index.html → build/index.html"
    VERBATIM
)
# ALL makes "make" include this target; we deliberately do NOT add it as a
# dependency of gardn-bundle, because that would force a multi-second wasm
# relink on every HTML edit.
add_custom_target(gardn-bundle-html ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/index.html"
)
# Embed Bots/model.onnx inside the wasm via --embed-file so the deliverable
# is a single file. Subtlety: we want
#   (a) the .onnx to be picked up by the next `make` even if it was produced
#       AFTER `cmake ..` (so user can do `python export_onnx.py && make`),
#   (b) the wasm to RE-LINK when the .onnx changes (so bundling actually
#       captures the new bytes), and
#   (c) NO relink when the .onnx is unchanged (relinks are ~2s).
#
# Mechanism: a "phony" custom command output forces stage_model.cmake to
# run on every `make`. That script writes build/embed/model.onnx — either
# the real bytes or an empty placeholder — only when content changes (via
# file(COPY_FILE ONLY_IF_DIFFERENT)). The bundle's LINK_DEPENDS on the
# staged file then drives relink-on-change.
# --- Bundled ONNX policy — staging pipeline ----------------------------------
# Mechanism: gardn-bundle-stage-model is a custom_target with ALL, so make
# invokes its COMMAND on every build — even when no inputs changed. The
# command runs stage_model.cmake, which only bumps STAGED_MODEL's mtime if
# the contents actually changed (file(COPY_FILE ... ONLY_IF_DIFFERENT) for
# present models; bare file(WRITE) skipped when an empty placeholder is
# already in place). LINK_DEPENDS on STAGED_MODEL then triggers a relink
# IFF the model changed. Cost: ~50 ms script run per `make`; benefit:
# `python export_onnx.py && make` Just Works without `cmake ..` between.
#
# A custom_command(OUTPUT) would NOT work here: CMake only re-runs OUTPUT
# commands when the file is missing or older than DEPENDS, and there's no
# native "always stale" attribute, so freshly-produced models would be
# silently skipped.
add_custom_target(gardn-bundle-stage-model ALL
    COMMAND ${CMAKE_COMMAND}
            -DSRC=${REPO_ROOT}/Bots/model.onnx
            -DDST=${STAGED_MODEL}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/stage_model.cmake
    BYPRODUCTS "${STAGED_MODEL}"
    COMMENT "Stage model.onnx for --embed-file"
    VERBATIM
)
