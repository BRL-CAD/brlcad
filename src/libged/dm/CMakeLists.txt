set(PLUGIN_LIBS libged libdm libbu)

set(DM_SRCS
  dm.c
  ert.cpp
  screengrab.c
)

ged_plugin_library(ged-dm
  ${DM_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${DM_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
