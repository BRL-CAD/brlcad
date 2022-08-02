include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-set_transparency SHARED set_transparency.c)
target_link_libraries(ged-set_transparency libged libbu)
set_property(TARGET ged-set_transparency APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-set_transparency set_transparency.c)
PLUGIN_SETUP(ged-set_transparency ged)

CMAKEFILES(
  CMakeLists.txt
  set_transparency.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
