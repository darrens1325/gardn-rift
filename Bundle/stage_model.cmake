# Helper invoked from Bundle/CMakeLists.txt to stage Bots/model.onnx next
# to the bundle on every `make` invocation. We deliberately re-run on
# every build (the target is OUTPUT-less) so a model produced *after* the
# CMake configure step still gets picked up by the next `make`. The script
# no-ops gracefully when the source is absent so users without a trained
# checkpoint don't see a hard error.
#
# Inputs (via -D):
#   SRC   absolute path to Bots/model.onnx
#   DST   where to copy it (build/model.onnx)

if (EXISTS "${SRC}")
    file(COPY_FILE "${SRC}" "${DST}" ONLY_IF_DIFFERENT)
    message(STATUS "[gardn-bundle] staged model.onnx → ${DST}")
else()
    message(STATUS "[gardn-bundle] no Bots/model.onnx; bots will use random actions (run `python Bots/export_onnx.py` to generate)")
endif()
