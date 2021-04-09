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
  diff.c
  hash.c
  polygons.c
  util.c
  )
BRLCAD_ADDLIB(libbview "${LIBBVIEW_SRCS}" "libbg;libbn;libbu")
set_target_properties(libbview PROPERTIES VERSION 20.0.1 SOVERSION 20)

set(libbg_ignore_files
  CMakeLists.txt
  )
CMAKEFILES(${libbview_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
