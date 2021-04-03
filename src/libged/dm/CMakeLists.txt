include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(DM_SRCS
  dm.c
  screengrab.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-dm SHARED ${DM_SRCS})
target_link_libraries(ged-dm libged libdm libbu)
set_property(TARGET ged-dm APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-dm ${DM_SRCS})
PLUGIN_SETUP(ged-dm ged)

CMAKEFILES(
  CMakeLists.txt
  ${DM_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
