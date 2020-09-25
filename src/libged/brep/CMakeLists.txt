include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(BREP_SRCS
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
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

