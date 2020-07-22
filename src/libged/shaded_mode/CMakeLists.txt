include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-shaded_mode SHARED shaded_mode.c)
target_link_libraries(ged-shaded_mode libged libbu)
set_property(TARGET ged-shaded_mode APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-shaded_mode shaded_mode.c)
PLUGIN_SETUP(ged-shaded_mode ged)

CMAKEFILES(
  CMakeLists.txt
  shaded_mode.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
