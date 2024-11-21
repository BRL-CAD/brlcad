include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(arb_srcs arb.c rotate_arb_face.c)

ged_plugin_library(ged-arb ${arb_srcs})
target_link_libraries(ged-arb libged libbu)

cmakefiles(
  CMakeLists.txt
  ${arb_srcs}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
