include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(DRAW_SRCS bigE.c draw.c draw2.cpp loadview.cpp preview.cpp)

ged_plugin_library(ged-draw ${DRAW_SRCS})
target_link_libraries(ged-draw libged libbu)

cmakefiles(
  CMakeLists.txt
  ged_draw.h
  ${DRAW_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
