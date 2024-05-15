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

BRLCAD_Find_Package(Geogram REQUIRED SYSPATTERN geogram)
add_definitions(-DUSE_GEOGRAM)

set(INCLUDE_DIRS
  ${GEOGRAM_INCLUDE_DIR}
  ${MANIFOLD_INCLUDE_DIRS}
  ${ASSETIMPORT_INCLUDE_DIR}
)

ged_subprocess_library(facetize_process SHARED ${GED_TESS_SRCS})
target_include_directories(facetize_process SYSTEM PRIVATE ${INCLUDE_DIRS})
target_link_libraries(facetize_process libged;${MANIFOLD_LIBRARIES};${ASSETIMPORT_LIBRARIES};${GEOGRAM_LIBRARY})
set_property(TARGET facetize_process APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
VALIDATE_STYLE(facetize_process ${GED_TESS_SRCS})
PLUGIN_SETUP(facetize_process ged_exec)

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
