include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-screengrab SHARED screengrab.c)
target_link_libraries(ged-screengrab libged libdm libbu)
set_property(TARGET ged-screengrab APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-screengrab screengrab.c)
PLUGIN_SETUP(ged-screengrab ged)

CMAKEFILES(
  CMakeLists.txt
  screengrab.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
