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

find_package(LEMON)
PERPLEX_TARGET(csg_scanner csg_scanner.perplex)
LEMON_TARGET(csg_parser csg_parser.lemon)
ADD_PERPLEX_LEMON_DEPENDENCY(csg_scanner csg_parser)

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/csg_parser_csg_parser/csg_parser.lemon)

set(CSG_SRCS
  csg.c
  ${PERPLEX_csg_scanner_SRC}
  ${LEMON_csg_parser_SRC}
  )

BRLCAD_ADDEXEC(csg "${CSG_SRCS}" "libgcv;libbu" NO_INSTALL)
set_property(TARGET csg APPEND PROPERTY INCLUDE_DIRECTORIES "${PERPLEX_csg_scanner_INCLUDE_DIR}")
set_property(TARGET csg APPEND PROPERTY INCLUDE_DIRECTORIES "${LEMON_csg_parser_INCLUDE_DIR}")


CMAKEFILES(csg.h csg_parser.lemon csg_scanner.perplex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
