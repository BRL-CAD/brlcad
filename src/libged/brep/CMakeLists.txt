
set(DPLOT_READER dplot)

PERPLEX_TARGET(
  ${DPLOT_READER}_scanner
  ${DPLOT_READER}_scanner.perplex
  OUT_SRC_FILE ${DPLOT_READER}_scanner.c
  OUT_HDR_FILE ${DPLOT_READER}_scanner.h)
LEMON_TARGET(
  ${DPLOT_READER}_parser
  ${DPLOT_READER}_parser.lemon
  OUT_SRC_FILE ${DPLOT_READER}_parser.c
  OUT_HDR_FILE ${DPLOT_READER}_parser.h)
ADD_PERPLEX_LEMON_DEPENDENCY(${DPLOT_READER}_scanner ${DPLOT_READER}_parser)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/${DPLOT_READER}_parser_${DPLOT_READER}_parser/${DPLOT_READER}_parser.lemon)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PERPLEX_${DPLOT_READER}_scanner_INCLUDE_DIR}
  ${LEMON_${DPLOT_READER}_parser_INCLUDE_DIR}
  )

set(BREP_SRCS
  ${PERPLEX_${DPLOT_READER}_scanner_SRC}
  ${LEMON_${DPLOT_READER}_parser_SRC}
  dplot.c
  brep.cpp
  conversion.cpp
  csg.cpp
  info.cpp
  intersect.cpp
  pick.cpp
  plot.cpp
  tikz.cpp
  valid.cpp
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-brep SHARED ${BREP_SRCS})
target_link_libraries(ged-brep libged libbu)
set_property(TARGET ged-brep APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-brep ${BREP_SRCS})
PLUGIN_SETUP(ged-brep ged)

CMAKEFILES(
  CMakeLists.txt
  ${BREP_SRCS}
  dplot_parser.lemon
  dplot_reader.c
  dplot_reader.h
  dplot_scanner.perplex
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

