set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-garbage_collect
  garbage_collect.cpp
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  garbage_collect.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
