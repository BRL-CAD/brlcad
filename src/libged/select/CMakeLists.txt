set(PLUGIN_LIBS libged libbu)

set(SELECT_SRCS
  select.c
  select2.cpp
)

ged_plugin_library(ged-select
  ${SELECT_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  ${SELECT_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
