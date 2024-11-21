include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(rot_srcs rot.c rotate_about.c)

ged_plugin_library(ged-rot ${rot_srcs})
target_link_libraries(ged-rot libged libbu)

cmakefiles(
  CMakeLists.txt
  ${rot_srcs}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
