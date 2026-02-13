set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-solids_on_ray
  solids_on_ray.c
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  solids_on_ray.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
