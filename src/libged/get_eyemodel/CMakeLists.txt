include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-get_eyemodel SHARED get_eyemodel.c)
target_link_libraries(ged-get_eyemodel libged libbu)
set_property(TARGET ged-get_eyemodel APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-get_eyemodel get_eyemodel.c)
PLUGIN_SETUP(ged-get_eyemodel ged)

CMAKEFILES(
  CMakeLists.txt
  get_eyemodel.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
