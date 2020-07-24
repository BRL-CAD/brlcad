include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-glob SHARED glob.c)
target_link_libraries(ged-glob libged libbu)
set_property(TARGET ged-glob APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-glob glob.c)
PLUGIN_SETUP(ged-glob ged)

CMAKEFILES(
  CMakeLists.txt
  glob.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
