# Pre-`project()` setup. CMAKE_TOOLCHAIN_FILE and CMAKE_SYSTEM_NAME must be
# resolved here, before project() runs the compiler-detection probe.
#
# For WASM builds, auto-load Emscripten's toolchain file so plain
# `cmake -DWASM_SERVER=1 ..` works equivalently to `emcmake cmake ..`. We
# search $EMSDK first, then fall back to `em-config EMSCRIPTEN_ROOT`. Without
# this, Apple Clang gets picked up before line 74 has a chance to switch the
# compiler to em++, and configuration fails with "library 'System' not found"
# (because we set CMAKE_SYSTEM_NAME=Generic to skip platform detection).
if (WASM_SERVER AND NOT CMAKE_TOOLCHAIN_FILE)
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
            CACHE FILEPATH "Emscripten toolchain (auto-detected from WASM_SERVER=1)" FORCE)
        message(STATUS "WASM_SERVER=1 auto-loaded Emscripten toolchain: ${CMAKE_TOOLCHAIN_FILE}")
    else()
        message(FATAL_ERROR
            "WASM_SERVER=1 requires the Emscripten toolchain, which could not be "
            "auto-located. Either run `emcmake cmake -DWASM_SERVER=1 ..` or set "
            "EMSDK in your environment so this CMakeLists can find Emscripten.cmake.")
    endif()
endif()

# Native builds on macOS need real platform detection so the linker can find
# libSystem. Only flag the build as a Generic cross-compile for WASM; the
# Emscripten toolchain (loaded above or by emcmake) overrides the system name
# to Emscripten anyway, so this is just defensive.
if (WASM_SERVER)
    set(CMAKE_SYSTEM_NAME Generic)
endif()

cmake_minimum_required(VERSION 3.16)
project(gardn-server)

include_directories(..)

# Derive ARENA_WIDTH / ARENA_HEIGHT from Map/main/main.tmj at configure
# time. Writes Shared/MapDimensions.hh into the build tree; the include
# path below makes <Shared/MapDimensions.hh> resolve to it.
include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/MapDims.cmake)
gardn_derive_arena_dims("${CMAKE_CURRENT_SOURCE_DIR}/../Map/main/main.tmj")
include_directories(${GARDN_MAP_DIMS_INCLUDE_DIR})

set(SOURCES
    EntityFunctions/Damage.cc
    EntityFunctions/Death.cc
    EntityFunctions/Detection.cc
    EntityFunctions/Misc.cc
    Process/Ai.cc
    Process/Camera.cc
    Process/Collision.cc
    Process/Culling.cc
    Process/Flower.cc
    Process/Health.cc
    Process/Motion.cc
    Process/Petal.cc
    Process/Score.cc
    Process/Segment.cc
    Client.cc
    Game.cc
    Main.cc
    PetalTracker.cc
    Server.cc
    Simulation.cc
    Spawn.cc
    TeamManager.cc
    TiledMap.cc
    ../Shared/Arena.cc
    ../Shared/Binary.cc
    ../Shared/Config.cc
    ../Shared/Entity.cc
    ../Shared/EntityDef.cc
    ../Shared/Helpers.cc
    ../Shared/Map.cc
    ../Shared/Simulation.cc
    ../Shared/StaticData.cc
    ../Shared/Vector.cc
)

if(WASM_SERVER)
    set(SOURCES ${SOURCES} Wasm.cc)
else()
    set(SOURCES ${SOURCES} Native.cc)
endif()
if(GENERAL_SPATIAL_HASH)
    set(SOURCES ${SOURCES} SpatialHashCanonical.cc)
else()
    set(SOURCES ${SOURCES} SpatialHashUniform.cc)
endif()
if(DEBUG)
    set(CMAKE_CXX_FLAGS "-gdwarf-4 -DDEBUG=1")
else()
    set(CMAKE_CXX_FLAGS "-O3 -ffast-math")
endif()
if (TDM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -DGAMEMODE_TDM=1")
endif()
if (TPS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGARDN_TPS=${TPS}")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20 -DSERVERSIDE=1")

if(WASM_SERVER)
    set(CMAKE_CXX_COMPILER "em++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWASM_SERVER=1")
    add_link_options(-sEXIT_RUNTIME=0 -sEXPORTED_FUNCTIONS=_main,_on_connect,_on_disconnect,_tick,_on_message)
    if (NOT DEBUG) 
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --closure=1")
    endif()
    add_executable(gardn-server ${SOURCES})
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
else()
    set(CMAKE_CXX_COMPILER "g++")
    add_executable(gardn-server ${SOURCES})
    target_include_directories(gardn-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/src)
    target_include_directories(gardn-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets/src)
    target_link_directories(gardn-server PRIVATE ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets)
    # Homebrew installs libuv outside Apple Clang's default search path. Probe
    # for `brew --prefix` and add its include/lib dirs so -luv resolves.
    if(APPLE)
        execute_process(COMMAND brew --prefix
                        OUTPUT_VARIABLE HOMEBREW_PREFIX
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
        if(HOMEBREW_PREFIX)
            target_include_directories(gardn-server PRIVATE ${HOMEBREW_PREFIX}/include)
            target_link_directories(gardn-server PRIVATE ${HOMEBREW_PREFIX}/lib)
        endif()
    endif()
    target_link_libraries(gardn-server uv z)
    # The uSockets archive is named `uSockets.a` (no `lib` prefix), and Apple's
    # ld doesn't accept GNU's `-l:filename` extension. Pass the full path so
    # both Apple and GNU linkers resolve it.
    target_link_libraries(gardn-server ${CMAKE_SOURCE_DIR}/uWebSockets/uSockets/uSockets.a)
    if(CMAKE_HOST_WIN32)
        target_link_libraries(gardn-server ws2_32)
    endif()
endif()