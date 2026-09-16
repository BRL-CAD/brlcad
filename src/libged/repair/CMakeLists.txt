set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-repair
  repair.cpp
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
)
