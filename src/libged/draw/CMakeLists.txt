set(PLUGIN_LIBS libged libbu)

set(DRAW_SRCS
  bigE.c
  draw.c
  draw2.cpp
  loadview.cpp
  preview.cpp
)

ged_plugin_library(ged-draw
  ${DRAW_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${PLUGIN_LIBS}
)

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
