set(GED_TESS_SRCS
  brep_csg.cpp
  co3ne.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

BRLCAD_Find_Package(MANIFOLD REQUIRED)

BRLCAD_Find_Package(ASSETIMPORT)
if(ASSETIMPORT_LIBRARIES)
  add_definitions(-DUSE_ASSETIMPORT)
endif(ASSETIMPORT_LIBRARIES)

BRLCAD_Find_Package(GEOGRAM SYSPATTERN geogram)
if(GEOGRAM_LIBRARY)
  add_definitions(-DUSE_GEOGRAM)
endif(GEOGRAM_LIBRARY)

set(INCLUDE_DIRS
  ${GEOGRAM_INCLUDE_DIR}
  ${MANIFOLD_INCLUDE_DIRS}
  ${ASSETIMPORT_INCLUDE_DIR}
)

BRLCAD_ADDEXEC(ged_tessellate "${GED_TESS_SRCS}" "libged;${MANIFOLD_LIBRARIES};${ASSETIMPORT_LIBRARIES};${GEOGRAM_LIBRARY}")
target_include_directories(ged_tessellate SYSTEM PRIVATE ${INCLUDE_DIRS})

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
