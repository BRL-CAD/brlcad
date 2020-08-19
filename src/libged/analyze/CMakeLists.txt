include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(analyze_srcs
  analyze.cpp
  arb8.cpp
  arbn.cpp
  ars.cpp
  superell.cpp
  sketch.cpp
  util.cpp
  op_pnts_vol.cpp
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-analyze SHARED ${analyze_srcs})
target_link_libraries(ged-analyze libged libanalyze libbu)
set_property(TARGET ged-analyze APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-analyze analyze.c)
PLUGIN_SETUP(ged-analyze ged)

CMAKEFILES(
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
