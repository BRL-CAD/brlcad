set(FACETIZE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(FACETIZE_LOCAL_INCLUDE_DIRS)

# https://github.com/elalish/manifold
set(MANIFOLD_INSTALL_DIR "")

set(MANIFOLD_INCLUDE_DIRS "${MANIFOLD_INSTALL_DIR}/include")
if(NOT "${MANIFOLD_INCLUDE_DIRS}" EQUAL "")
  set(FACETIZE_LOCAL_INCLUDE_DIRS
    ${FACETIZE_LOCAL_INCLUDE_DIRS}
    ${MANIFOLD_INCLUDE_DIRS}
    )
endif(NOT "${MANIFOLD_INCLUDE_DIRS}" EQUAL "")

set(FACETIZE_LIBS libged libbu)

set(MANIFOLD_LIBRARIES
  ${MANIFOLD_INSTALL_DIR}/lib/libmanifold.so
  ${MANIFOLD_INSTALL_DIR}/lib/libcross_section.so
  ${MANIFOLD_INSTALL_DIR}/lib/libcollider.so
  ${MANIFOLD_INSTALL_DIR}/lib/libpolygon.so
  ${MANIFOLD_INSTALL_DIR}/lib/libquickhull.so
  )
if (NOT "${MANIFOLD_LIBRARIES}" EQUAL "")
  set(FACETIZE_LIBS ${FACETIZE_LIBS} ${MANIFOLD_LIBRARIES})
endif (NOT "${MANIFOLD_LIBRARIES}" EQUAL "")

# We want to include 3rd party libraries with -isystem - use
# BRLCAD_LIB_INCLUDE_DIRS for that purpose
BRLCAD_LIB_INCLUDE_DIRS(facetize FACETIZE_INCLUDE_DIRS FACETIZE_LOCAL_INCLUDE_DIRS)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-facetize SHARED facetize.cpp facetize_log.c)
target_link_libraries(ged-facetize ${FACETIZE_LIBS})
set_property(TARGET ged-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-facetize facetize.cpp)
PLUGIN_SETUP(ged-facetize ged)

CMAKEFILES(
  CMakeLists.txt
  facetize.cpp
  facetize_log.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
