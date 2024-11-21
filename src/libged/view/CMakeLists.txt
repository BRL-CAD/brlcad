include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(
  VIEW_SRCS
  aet.c
  align.c
  autoview.c
  autoview2.cpp
  axes.c
  center.cpp
  eye.c
  faceplate/axes.c
  faceplate/faceplate.c
  faceplate/grid.c
  faceplate/interactive_rect.c
  gobjs.cpp
  labels.c
  lines.c
  lod.cpp
  lookat.c
  objs.cpp
  polygons.c
  quat.c
  qvrot.c
  saveview.c
  size.c
  snap.c
  view.c
  viewdir.c
  ypr.c
)

ged_plugin_library(ged-view ${VIEW_SRCS})
target_link_libraries(ged-view libged libbu)

cmakefiles(
  CMakeLists.txt
  ${VIEW_SRCS}
  ged_view.h
  faceplate/faceplate.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
