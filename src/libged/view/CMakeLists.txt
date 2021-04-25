include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(VIEW_SRCS
  aet.c
  axes.c
  center.cpp
  eye.c
  faceplate.c
  labels.c
  lines.c
  objs.c
  polygons.c
  quat.c
  size.c
  snap.c
  view.c
  ypr.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-view SHARED ${VIEW_SRCS})
target_link_libraries(ged-view libged libbu)
set_property(TARGET ged-view APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-view ${VIEW_SRCS})
PLUGIN_SETUP(ged-view ged)

CMAKEFILES(
  CMakeLists.txt
  ${VIEW_SRCS}
  ged_view.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

