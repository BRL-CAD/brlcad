set(PLUGIN_LIBS libged libbu)

set(RTCHECK_SRCS
  rtcheck.c
  rtcheck2.cpp
)

ged_plugin_library(ged-rtcheck
  ${RTCHECK_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  rtcheck.c
  rtcheck2.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
