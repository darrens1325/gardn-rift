# Derives ARENA_WIDTH / ARENA_HEIGHT from a Tiled .tmj at CMake configure
# time and writes them into a generated header that StaticDefinitions.hh
# includes. The arena should always be the full extent of the map; baking
# the values at configure-time keeps every spatial-hash / clamp / minimap
# call site working with compile-time constants while removing the manual
# "remember to update ARENA_WIDTH after editing the map" footgun.
#
# Usage from a project CMakeLists:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../Shared/MapDims.cmake)   # or wherever
#   gardn_derive_arena_dims("${CMAKE_CURRENT_SOURCE_DIR}/../Map/main/main.tmj")
#   include_directories(${GARDN_MAP_DIMS_INCLUDE_DIR})
#
# Re-runs whenever the .tmj's mtime changes (CMAKE_CONFIGURE_DEPENDS).

# Capture this file's directory at include time. CMAKE_CURRENT_LIST_DIR is
# re-bound every time CMake enters/leaves a file, including when the
# function body is executed inside the caller's listfile — so reading it
# from inside the function would point at the caller, not at Shared/.
set(_GARDN_MAP_DIMS_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/MapDimensions.hh.in")

function(gardn_derive_arena_dims tmj_path)
    if (NOT EXISTS "${tmj_path}")
        message(FATAL_ERROR "gardn_derive_arena_dims: map file not found: ${tmj_path}")
    endif()

    # Tell CMake to reconfigure when the map changes so the generated
    # header stays in sync without a manual `cmake ..` re-run.
    set_property(DIRECTORY APPEND PROPERTY
        CMAKE_CONFIGURE_DEPENDS "${tmj_path}")

    file(READ "${tmj_path}" _tmj)

    # The .tmj has nested objects with their own "width" / "height" keys
    # (each tile layer carries the grid dimensions, plus per-object boxes
    # under objectgroup layers). We can't slice off "layers" onward
    # because Tiled emits keys alphabetically, so "tileheight" /
    # "tilewidth" / "width" all come AFTER "layers" at the top level.
    # Instead, anchor on indent depth: Tiled writes top-level fields
    # with exactly one leading space, nested fields with more.
    set(_fields width height tilewidth tileheight)
    foreach(_f IN LISTS _fields)
        if (NOT _tmj MATCHES "[\r\n] \"${_f}\"[ \t]*:[ \t]*([0-9]+)")
            message(FATAL_ERROR
                "gardn_derive_arena_dims: could not find top-level "
                "\"${_f}\" in ${tmj_path}")
        endif()
        set(_${_f} "${CMAKE_MATCH_1}")
    endforeach()

    math(EXPR _arena_w "${_width} * ${_tilewidth}")
    math(EXPR _arena_h "${_height} * ${_tileheight}")

    if (_arena_w LESS_EQUAL 0 OR _arena_h LESS_EQUAL 0)
        message(FATAL_ERROR
            "gardn_derive_arena_dims: computed non-positive arena size "
            "(${_arena_w} x ${_arena_h}) from ${tmj_path}")
    endif()

    set(GARDN_ARENA_WIDTH  "${_arena_w}")
    set(GARDN_ARENA_HEIGHT "${_arena_h}")

    set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(_out_path "${_out_dir}/Shared/MapDimensions.hh")
    file(MAKE_DIRECTORY "${_out_dir}/Shared")

    configure_file("${_GARDN_MAP_DIMS_TEMPLATE}" "${_out_path}" @ONLY)

    message(STATUS
        "gardn_derive_arena_dims: ARENA=${_arena_w}x${_arena_h} "
        "(map ${_width}x${_height} tiles, ${_tilewidth}x${_tileheight}px each) "
        "→ ${_out_path}")

    set(GARDN_MAP_DIMS_INCLUDE_DIR "${_out_dir}" PARENT_SCOPE)
endfunction()
