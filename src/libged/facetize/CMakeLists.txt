set(FACETIZE_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(FACETIZE_LOCAL_INCLUDE_DIRS)
if(NOT "${IRMB_INCLUDE_DIRS}" EQUAL "")
  set(FACETIZE_LOCAL_INCLUDE_DIRS
    ${FACETIZE_LOCAL_INCLUDE_DIRS}
    ${IRMB_INCLUDE_DIRS}
    )
endif(NOT "${IRMB_INCLUDE_DIRS}" EQUAL "")

set(FACETIZE_LIBS libged libbu)

if (NOT "${IRMB_LIBRARIES}" EQUAL "")
  set(FACETIZE_LIBS ${FACETIZE_LIBS} ${IRMB_LIBRARIES})
endif (NOT "${IRMB_LIBRARIES}" EQUAL "")

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
