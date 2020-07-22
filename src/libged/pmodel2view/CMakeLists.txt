include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-pmodel2view SHARED pmodel2view.c)
target_link_libraries(ged-pmodel2view libged libbu)
set_property(TARGET ged-pmodel2view APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-pmodel2view pmodel2view.c)
PLUGIN_SETUP(ged-pmodel2view ged)

CMAKEFILES(
  CMakeLists.txt
  pmodel2view.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
