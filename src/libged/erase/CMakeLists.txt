set(PLUGIN_LIBS libged libbu)

set(ERASE_SRCS
  erase.c
  erase2.cpp
  )

ged_plugin_library(ged-erase
  ${ERASE_SRCS}
  LIBS ${PLUGIN_LIBS}
)

cmakefiles(
  CMakeLists.txt
  erase.c
  erase2.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
