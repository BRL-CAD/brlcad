include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-eye_pos SHARED eye_pos.c)
target_link_libraries(ged-eye_pos libged libbu)
set_property(TARGET ged-eye_pos APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-eye_pos eye_pos.c)
PLUGIN_SETUP(ged-eye_pos ged)

CMAKEFILES(
  CMakeLists.txt
  eye_pos.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
