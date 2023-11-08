set(FACETIZE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )
set(FACETIZE_LOCAL_INCLUDE_DIRS
  ${MANIFOLD_INCLUDE_DIRS}
  )

# We want to include 3rd party libraries with -isystem - use
# BRLCAD_LIB_INCLUDE_DIRS for that purpose
BRLCAD_LIB_INCLUDE_DIRS(facetize FACETIZE_INCLUDE_DIRS FACETIZE_LOCAL_INCLUDE_DIRS)

set(FACETIZE_SRCS
  brep.cpp
  continuation.cpp
  facetize.cpp
  facetize_log.c
  manifold.cpp
  nmg.cpp
  regions.cpp
  spsr.cpp
  util.cpp
  )

set(FACETIZE_LIBS libged libbu ${MANIFOLD_LIBRARIES})

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-facetize SHARED ${FACETIZE_SRCS})
target_link_libraries(ged-facetize ${FACETIZE_LIBS})
set_property(TARGET ged-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
VALIDATE_STYLE(ged-facetize facetize.cpp)
PLUGIN_SETUP(ged-facetize ged)

CMAKEFILES(
  CMakeLists.txt
  ged_facetize.h
  ${FACETIZE_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
