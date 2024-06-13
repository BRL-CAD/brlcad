BRLCAD_Find_Package(LMDB REQUIRED)

set(LIBBV_SRCS
  adc.c
  diff.c
  font.c
  hash.c
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

set(BV_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LMDB_INCLUDE_DIR}
  )

# Note - libbv_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
BRLCAD_ADDLIB(libbv "${LIBBV_SRCS}" "${libbv_deps};${LMDB_LIBRARIES}" "" "${BV_LOCAL_INCLUDE_DIRS}")
set_target_properties(libbv PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libbv APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  if (TARGET libbv-obj)
    set_property(TARGET libbv-obj APPEND PROPERTY COMPILE_DEFINITIONS "PLOT3_DLL_EXPORTS")
  endif (TARGET libbv-obj)
endif(HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

CMAKEFILES(
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
