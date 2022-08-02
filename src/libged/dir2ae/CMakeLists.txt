include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-dir2ae SHARED dir2ae.c)
target_link_libraries(ged-dir2ae libged libbu)
set_property(TARGET ged-dir2ae APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-dir2ae dir2ae.c)
PLUGIN_SETUP(ged-dir2ae ged)

CMAKEFILES(
  CMakeLists.txt
  dir2ae.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
