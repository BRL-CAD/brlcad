set(CSG_INCLUDE_DIRS
  ${GCV_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(CSG_INCLUDE_DIRS)

# Also want local dirs
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

PERPLEX_TARGET(csg_scanner csg_scanner.perplex
  ${CMAKE_CURRENT_BINARY_DIR}/csg_scanner.c
  ${CMAKE_CURRENT_BINARY_DIR}/csg_scanner.h)
LEMON_TARGET(csg_parser csg_parser.lemon
  ${CMAKE_CURRENT_BINARY_DIR}/csg_parser.c
  ${CMAKE_CURRENT_BINARY_DIR}/csg_parser.h)
ADD_PERPLEX_LEMON_DEPENDENCY(csg_scanner csg_parser)

set(CSG_SRCS
  csg.c
  ${CMAKE_CURRENT_BINARY_DIR}/csg_scanner.c
  ${CMAKE_CURRENT_BINARY_DIR}/csg_parser.c
  )

BRLCAD_ADDEXEC(csg "${CSG_SRCS}" "libgcv;libbu" NO_INSTALL)

CMAKEFILES(csg.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
