set(PLUGIN_LIBS libged libbu)

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
  knob.c
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

ged_plugin_library(ged-view
  ${VIEW_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${PLUGIN_LIBS}
)

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
