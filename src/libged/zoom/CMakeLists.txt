include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(ZOOM_SRCS
  zoom.c
  )

add_definitions(-DGED_PLUGIN)

add_library(ged-zoom SHARED ${ZOOM_SRCS})
target_link_libraries(ged-zoom libged libbu)
set_property(TARGET ged-zoom APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-zoom "${ZOOM_SRCS}")

PLUGIN_SETUP(ged-zoom ged)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
