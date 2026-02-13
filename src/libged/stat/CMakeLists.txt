set(PLUGIN_LIBS libged libbu)

set(STAT_SRCS
  stat.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../libbu/fort.c
  )

ged_plugin_library(ged-stat
  ${STAT_SRCS}
  INCLUDES ${BRLCAD_SOURCE_DIR}/src/libbu # for fort.h
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  stat.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
