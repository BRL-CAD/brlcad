include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(cg_srcs
  cg.cpp
  eval_pnts.cpp
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-cg SHARED ${cg_srcs})
target_link_libraries(ged-cg libged libbu)
set_property(TARGET ged-cg APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-cg cg.cpp)
PLUGIN_SETUP(ged-cg ged)

CMAKEFILES(
  ${cg_srcs}
  CMakeLists.txt
  ged_cg.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
