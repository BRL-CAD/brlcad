set(
  LIBBV_SRCS
  adc.c
  diff.c
  font.c
  hash.c
  knobs.cpp
  lod.cpp
  polygon.c
  polygon_op.cpp
  polygon_fill.cpp
  snap.c
  tig/axis.c
  tig/list.c
  tig/marker.c
  tig/scale.c
  tig/symbol.c
  tig/tplot.c
  tig/vectfont.c
  tig/vector.c
  util.cpp
  vlist.c
  view_sets.cpp
)

set(BV_LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Note - libbv_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libbv "${LIBBV_SRCS}" "${libbv_deps}" "" "${BV_LOCAL_INCLUDE_DIRS}"
  # vlist.c defines PLOT3_IMPLEMENTATION before including bv/plot3.h, which
  # causes the plot3_* function bodies to be compiled into this translation
  # unit.  If vlist.c is batched with files that include bv/plot3.h first,
  # the header guard (PLOT3_H) prevents re-entry and the function bodies are
  # never compiled, producing undefined-reference link errors.
  UNITY_BUILD_SKIP vlist.c
)
set_target_properties(libbv PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libbv APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  if(TARGET libbv-obj)
    set_property(TARGET libbv-obj APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  endif(TARGET libbv-obj)
endif(HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  TODO
  bv_private.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
