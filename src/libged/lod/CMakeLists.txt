set(PLUGIN_LIBS libged libbu)

set(LOD_SRCS
  lod.c
  lod2.cpp
  )

ged_plugin_library(ged-lod
  ${LOD_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  lod.c
  lod2.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
