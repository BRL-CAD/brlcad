# Include directories needed by libbview users
set(BVIEW_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${BG_INCLUDE_DIRS}
  )
# local includes
set(BVIEW_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(bview BVIEW_INCLUDE_DIRS BVIEW_LOCAL_INCLUDE_DIRS)

# Main libbview library
set(LIBBVIEW_SRCS
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
BRLCAD_ADDLIB(libbview "${LIBBVIEW_SRCS}" "libbn;libbu")
set_target_properties(libbview PROPERTIES VERSION 20.0.1 SOVERSION 20)

add_subdirectory(tests)

set(libbview_ignore_files
  CMakeLists.txt
  TODO
  )
CMAKEFILES(${libbview_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
