set(PLUGIN_LIBS libged libanalyze libbu)

set(
  analyze_srcs
  analyze.cpp
  arb8.cpp
  arbn.cpp
  ars.cpp
  superell.cpp
  sketch.cpp
  util.cpp
  op_pnts_vol.cpp
)

ged_plugin_library(ged-analyze
  ${analyze_srcs}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${analyze_srcs}
  ged_analyze.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
