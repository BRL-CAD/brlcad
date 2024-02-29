set(GED_TESS_SRCS
  co3ne.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

if(ASSETIMPORT_LIBRARIES)
  add_definitions(-DUSE_ASSETIMPORT)
endif(ASSETIMPORT_LIBRARIES)

if(GEOGRAM_LIBRARY)
  add_definitions(-DUSE_GEOGRAM)
endif(GEOGRAM_LIBRARY)

set(INCLUDE_DIRS
  ${GEOGRAM_INCLUDE_DIR}
  ${MANIFOLD_INCLUDE_DIRS}
  ${ASSETIMPORT_INCLUDE_DIR}
)
BRLCAD_INCLUDE_DIRS(INCLUDE_DIRS)

BRLCAD_ADDEXEC(ged_tessellate "${GED_TESS_SRCS}" "libged;${MANIFOLD_LIBRARIES};${ASSETIMPORT_LIBRARIES};${GEOGRAM_LIBRARY}")

CMAKEFILES(
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
