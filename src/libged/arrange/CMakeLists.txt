set(PLUGIN_LIBS libged libanalyze libwdb librt libbu)

ged_plugin_library(ged-arrange
  arrange.cpp
  nest.cpp
  nest_candidates.cpp
  nest_raster.cpp
  nest_search.cpp
  LIBS ${PLUGIN_LIBS}
)

add_subdirectory(tests)

cmakefiles(
  CMakeLists.txt
  arrange_private.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

