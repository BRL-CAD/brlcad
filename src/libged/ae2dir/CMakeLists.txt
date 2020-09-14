include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-ae2dir SHARED ae2dir.c)
target_link_libraries(ged-ae2dir libged libbu)
set_property(TARGET ged-ae2dir APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-ae2dir ae2dir.c)
PLUGIN_SETUP(ged-ae2dir ged)

CMAKEFILES(
  CMakeLists.txt
  ae2dir.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
