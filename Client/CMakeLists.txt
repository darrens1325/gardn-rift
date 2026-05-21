# Pre-`project()` setup. CMAKE_TOOLCHAIN_FILE and CMAKE_SYSTEM_NAME must be
# resolved before project() runs the compiler-detection probe. Without the
# Emscripten toolchain CMake picks Apple Clang (or whatever cc is on PATH),
# which then fails the test compile with "library 'System' not found"
# because we set CMAKE_SYSTEM_NAME=Generic to skip platform detection.
#
# Mirrors the auto-detect block in Server/CMakeLists.txt: search $EMSDK
# first, then `em-config EMSCRIPTEN_ROOT`, so plain `cmake ..` works
# equivalently to `emcmake cmake ..` as long as Emscripten is installed.
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
        message(STATUS "Auto-loaded Emscripten toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(FATAL_ERROR
            "gardn-client requires the Emscripten toolchain, which could not be "
            "auto-located. Either run `emcmake cmake ..` or set EMSDK in your "
            "environment so this CMakeLists can find Emscripten.cmake.")
    endif()
endif()

set(CMAKE_SYSTEM_NAME Generic)

cmake_minimum_required(VERSION 3.16)

project(gardn-client)
include_directories(..)
set(CMAKE_C_COMPILER "emcc")

# Derive ARENA_WIDTH / ARENA_HEIGHT from Map/main/main.tmj at configure
# time. Writes Shared/MapDimensions.hh into the build tree; the include
# path below makes <Shared/MapDimensions.hh> resolve to it.
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/MapDims.cmake)
gardn_derive_arena_dims("${CMAKE_CURRENT_SOURCE_DIR}/../Map/main/main.tmj")
include_directories(${GARDN_MAP_DIMS_INCLUDE_DIR})

set(SRCS
    Assets/Flower.cc
    Assets/Mob.cc
    Assets/Petal.cc
    Assets/Web.cc
    Render/RenderDrop.cc
    Render/RenderFlower.cc
    Render/RenderHealth.cc
    Render/RenderMob.cc
    Render/RenderName.cc
    Render/RenderPetal.cc
    Render/RenderWeb.cc
    Render/Renderer.cc
    Render/TiledMapRender.cc
    Debug.cc
    DOM.cc
    Game.cc
    Input.cc
    Main.cc
    Network.cc
    Particle.cc
    Rendering.cc
    Setup.cc
    Simulation.cc
    Socket.cc
    StaticData.cc
    Storage.cc
    Ui/InGame/InputFreeze.cc
    Ui/InGame/Leaderboard.cc
    Ui/InGame/LevelBar.cc
    Ui/InGame/LoadoutPetal.cc
    Ui/InGame/LoadoutSlot.cc
    Ui/InGame/Map.cc
    Ui/InGame/OverlevelTimer.cc
    Ui/InGame/RoundEndBanner.cc
    Ui/InGame/Tooltip.cc
    Ui/TitleScreen/Changelog.cc
    Ui/TitleScreen/DeathScreen.cc
    Ui/TitleScreen/MainScreen.cc
    Ui/TitleScreen/MobGallery.cc
    Ui/TitleScreen/PetalGallery.cc
    Ui/TitleScreen/Settings.cc
    Ui/TitleScreen/StatScreen.cc
    Ui/Button.cc
    Ui/Choose.cc
    Ui/Container.cc
    Ui/DynamicText.cc
    Ui/Element.cc
    Ui/Extern.cc
    Ui/ScrollContainer.cc
    Ui/StaticText.cc
    Ui/TextInput.cc
    Ui/Window.cc
    ../Shared/Arena.cc
    ../Shared/Binary.cc
    ../Shared/Config.cc
    ../Shared/Entity.cc
    ../Shared/EntityDef.cc
    ../Shared/Helpers.cc
    ../Shared/Map.cc
    ../Shared/MapDimensions.cc
    ../Shared/Simulation.cc
    ../Shared/StaticData.cc
    ../Shared/Vector.cc
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCLIENTSIDE=1 -std=c++20 -s WASM=0")
if (TPS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGARDN_TPS=${TPS}")
endif()
set(CMAKE_CXX_COMPILER "em++")
if(DEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=1 -gdwarf-4 -sNO_DISABLE_EXCEPTION_CATCHING")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math --closure=0")
endif()

add_link_options(-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_key_event,_mouse_event,_wheel_event,_loop,_on_message)

add_executable(gardn-client ${SRCS})
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# --- Stage Map/ and tiles/ next to the built client ---------------------------
# Client/Render/TiledMapRender.cc fetches map JSONs and tile SVGs over HTTP at
# runtime (`fetch("/Map/<dir>/<file>.tmj")`, `fetch("/tiles/foo.svg")`); for a
# plain `python -m http.server` in Client/build/ to serve the whole game,
# Map/ and tiles/ have to live alongside gardn-client.{js,wasm}. The default
# map is fetched at boot (TiledMapRender::init → load("Map/main/main.tmj"))
# and any `?spawn=Map/foo/foo.tmj` URL request triggers another fetch. Per-file
# copy_if_different keeps incremental builds cheap; CONFIGURE_DEPENDS makes
# cmake re-run when files are added or removed under Map/ or tiles/.
set(_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
file(GLOB_RECURSE _GARDN_MAP_ASSETS CONFIGURE_DEPENDS
     "${_REPO_ROOT}/Map/*"
     "${_REPO_ROOT}/tiles/*")
set(_GARDN_STAGED_ASSETS "")
foreach(_src IN LISTS _GARDN_MAP_ASSETS)
    file(RELATIVE_PATH _rel "${_REPO_ROOT}" "${_src}")
    set(_dst "${CMAKE_CURRENT_BINARY_DIR}/${_rel}")
    add_custom_command(
        OUTPUT "${_dst}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_src}" "${_dst}"
        DEPENDS "${_src}"
        VERBATIM
    )
    list(APPEND _GARDN_STAGED_ASSETS "${_dst}")
endforeach()
add_custom_target(gardn-client-assets ALL DEPENDS ${_GARDN_STAGED_ASSETS})
add_dependencies(gardn-client gardn-client-assets)
