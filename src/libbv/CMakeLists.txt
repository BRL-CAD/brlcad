# Include directories needed by libbv users
set(BV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${BG_INCLUDE_DIRS}
  )

set(BV_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(bv BV_INCLUDE_DIRS BV_LOCAL_INCLUDE_DIRS)

set(LIBBV_SRCS
  adc.c
  diff.c
  font.c
  hash.c
  plot3.c
  tig/axis.c
  tig/list.c
  tig/marker.c
  tig/scale.c
  tig/symbol.c
  tig/tplot.c
  tig/vectfont.c
  tig/vector.c
  util.c
  vlist.c
  )

BRLCAD_ADDLIB(libbv "${LIBBV_SRCS}" "libbn;libbu")
set_target_properties(libbv PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  TODO
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
