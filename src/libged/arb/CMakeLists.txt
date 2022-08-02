include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(arb_srcs
  arb.c
  rotate_arb_face.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-arb SHARED ${arb_srcs})
target_link_libraries(ged-arb libged libbu)
set_property(TARGET ged-arb APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-arb ${arb_srcs})
PLUGIN_SETUP(ged-arb ged)

CMAKEFILES(
  CMakeLists.txt
  ${arb_srcs}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
