set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-npush
  npush.cpp
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  npush_options.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
