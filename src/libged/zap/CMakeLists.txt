set(PLUGIN_LIBS libged libbu)

set(ZAP_SRCS
  zap.c
  zap2.cpp
)

ged_plugin_library(ged-zap
  ${ZAP_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${ZAP_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
