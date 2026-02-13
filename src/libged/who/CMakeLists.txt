set(PLUGIN_LIBS libged libbu)

set(WHO_SRCS
  who.c
  who2.cpp
)

ged_plugin_library(ged-who
  ${WHO_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${WHO_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
