brlcad_find_package(manifold REQUIRED)
set(TESS_LIBS libged manifold::manifold)

set(
  GED_TESS_SRCS
  brep_csg.cpp
  spsr.cpp
  main.cpp
  mdc.cpp
  nmg.cpp
  nmg_boolean.cpp
  util.cpp
  ../plan.cpp
  ../process.cpp
  ../tri.cpp
  ../transfer.cpp
  ../util.cpp
  ../validation.cpp
  ../worker.cpp
)

ged_subprocess_library(facetize_process
  ${GED_TESS_SRCS}
  LIBS ${TESS_LIBS}
)

cmakefiles(
  CMakeLists.txt
  nmg_boolean.h
  tessellate.h
  ${GED_TESS_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
