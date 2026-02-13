brlcad_find_package(manifold REQUIRED)
brlcad_find_package(Geogram REQUIRED SYSPATTERN geogram)
set(TESS_INCLUDE_DIRS ${Geogram_INCLUDE_DIR})
set(TESS_LIBS libged manifold::manifold ${Geogram_LIBRARY})

set(
  GED_TESS_SRCS
  brep_csg.cpp
  co3ne.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

ged_subprocess_library(facetize_process
  ${GED_TESS_SRCS}
  SYSTEM_INCLUDES ${TESS_INCLUDE_DIRS}
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
