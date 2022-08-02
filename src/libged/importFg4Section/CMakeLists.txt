include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-importFg4Section SHARED importFg4Section.c)
target_link_libraries(ged-importFg4Section libged libbu)
set_property(TARGET ged-importFg4Section APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-importFg4Section importFg4Section.c)
PLUGIN_SETUP(ged-importFg4Section ged)

CMAKEFILES(
  CMakeLists.txt
  importFg4Section.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
