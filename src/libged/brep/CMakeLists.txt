brlcad_find_package(PERPLEX REQUIRED NOINSTALL)
brlcad_find_package(LEMON REQUIRED NOINSTALL)
brlcad_find_package(RE2C REQUIRED NOINSTALL)

perplex_target(
  dplot_scanner
  dplot_scanner.perplex
  OUT_SRC_FILE dplot_scanner.c
  OUT_HDR_FILE dplot_scanner.h
)
lemon_target(
  dplot_parser
  dplot_parser.lemon
  OUT_SRC_FILE dplot_parser.c
  OUT_HDR_FILE dplot_parser.h
)
add_perplex_lemon_dependency(dplot_scanner dplot_parser)
distclean(${CMAKE_CURRENT_BINARY_DIR}/dplot_parser_dplot_parser/dplot_parser.lemon)

set(
  BREP_LIBS
  libged
  libbu
)

set(
  BREP_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set(
  BREP_LOCAL_INCLUDE_DIRS
  ${PERPLEX_dplot_scanner_INCLUDE_DIR}
  ${LEMON_dplot_parser_INCLUDE_DIR}
)

set(
  BREP_SRCS
  ${PERPLEX_dplot_scanner_SRC}
  ${LEMON_dplot_parser_SRC}
  brep.cpp
  conversion.cpp
  csg.cpp
  dplot.c
  geometry.cpp
  info.cpp
  intersect.cpp
  pick.cpp
  plot.cpp
  repair.cpp
  tikz.cpp
  topology.cpp
  valid.cpp
)

# Because ged-brep depends on the perplex/lemon generated sources for its content,
# its not suitable to build as part of libged proper - we would need to have
# the parent libged library depend on the local generated file targets.  Hence
# we mark this one as PLUGIN_ONLY
ged_plugin_library(ged-brep
  ${BREP_SRCS}
  INCLUDES ${BREP_INCLUDE_DIRS}
  SYSTEM_INCLUDES ${BREP_LOCAL_INCLUDE_DIRS}
  LIBS ${BREP_LIBS}
  PLUGIN_ONLY
)

# Suppress warnings from the generated source code
check_c_compiler_flag("-w"   HAVE_C_FLAG_W)
if (HAVE_C_FLAG_W)
  set_property(SOURCE ${LEMON_dplot_parser_SRC} APPEND PROPERTY COMPILE_OPTIONS "-w")
endif()

cmakefiles(
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
