set(PLUGIN_LIBS libged libbu)

set(STAT_SRCS
  stat.cpp
  )

ged_plugin_library(ged-stat
  ${STAT_SRCS}
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
