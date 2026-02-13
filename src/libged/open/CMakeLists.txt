set(OPEN_LIBS libged libbu)

ged_plugin_library(ged-open
  open.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${OPEN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  open.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
