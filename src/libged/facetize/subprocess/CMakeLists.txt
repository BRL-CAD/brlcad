brlcad_find_package(manifold REQUIRED)
set(TESS_LIBS libged manifold::manifold)

set(
  GED_TESS_SRCS
  brep_csg.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

ged_subprocess_library(facetize_process
  ${GED_TESS_SRCS}
  LIBS ${TESS_LIBS}
)

cmakefiles(
  CMakeLists.txt
  tessellate.h
  ${GED_TESS_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
