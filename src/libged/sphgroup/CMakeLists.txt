include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-sphgroup SHARED sphgroup.c)
target_link_libraries(ged-sphgroup libged libbu)
set_property(TARGET ged-sphgroup APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-sphgroup sphgroup.c)
PLUGIN_SETUP(ged-sphgroup ged)

CMAKEFILES(
  CMakeLists.txt
  sphgroup.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
