# Helper invoked from Bundle/CMakeLists.txt to stage a model.onnx file at
# DST that will then be --embed-file'd into the wasm. We re-run this on
# every `make` so a model produced AFTER `cmake ..` still gets picked up.
#
# A staged file always exists — either the real Bots/model.onnx, or an
# empty placeholder when no model has been exported. Bundle/index.html
# reads the embedded file from MEMFS and switches to random actions when
# it sees size 0.
#
# DST's mtime is the relink signal: CMake LINK_DEPENDS on this file makes
# `make` re-link the bundle when (and only when) the model changed. So we
# carefully avoid touching DST when content is unchanged.
#
# Inputs (via -D):
#   SRC   absolute path to Bots/model.onnx
#   DST   where to write it (build/embed/model.onnx)

cmake_minimum_required(VERSION 3.19)  # for file(SIZE)

get_filename_component(_dst_dir "${DST}" DIRECTORY)
file(MAKE_DIRECTORY "${_dst_dir}")

if (EXISTS "${SRC}")
    # file(COPY_FILE ... ONLY_IF_DIFFERENT) compares contents — no mtime
    # bump on identical content, so subsequent links are skipped.
    file(COPY_FILE "${SRC}" "${DST}" ONLY_IF_DIFFERENT)
    message(STATUS "[gardn-bundle] embedded model.onnx (from ${SRC})")
else()
    set(_need_write TRUE)
    if (EXISTS "${DST}")
        file(SIZE "${DST}" _dst_size)
        if (_dst_size EQUAL 0)
            set(_need_write FALSE)
        endif()
    endif()
    if (_need_write)
        file(WRITE "${DST}" "")
        message(STATUS "[gardn-bundle] no Bots/model.onnx; embedded empty placeholder (run `python Bots/export_onnx.py` then re-make to bundle the trained policy)")
    endif()
endif()
