if (BRLCAD_COMPONENTS OR NOT EXISTS ${CMAKE_SOURCE_DIR}/src/other)
  BRLCAD_Find_Package(MANIFOLD REQUIRED)
  BRLCAD_Find_Package(ASSETIMPORT)
endif (BRLCAD_COMPONENTS OR NOT EXISTS ${CMAKE_SOURCE_DIR}/src/other)

set(FACETIZE_LOCAL_INCLUDE_DIRS)
if (MANIFOLD_LIBRARIES)
  list(APPEND FACETIZE_LOCAL_INCLUDE_DIRS ${MANIFOLD_INCLUDE_DIRS})
  add_definitions(-DUSE_MANIFOLD)

  if (ASSETIMPORT_LIBRARY)
    list(APPEND FACETIZE_LOCAL_INCLUDE_DIRS ${ASSETIMPORT_INCLUDE_DIR})
    add_definitions(-DUSE_ASSETIMPORT)
  endif (ASSETIMPORT_LIBRARY)
endif (MANIFOLD_LIBRARIES)

set(FACETIZE_LIBS libged libbu ${MANIFOLD_LIBRARIES} ${ASSETIMPORT_LIBRARIES})

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-facetize SHARED facetize.cpp facetize_log.c)
target_include_directories(ged-facetize BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if (FACETIZE_LOCAL_INCLUDE_DIRS)
  target_include_directories(ged-facetize SYSTEM BEFORE PRIVATE ${FACETIZE_LOCAL_INCLUDE_DIRS})
endif (FACETIZE_LOCAL_INCLUDE_DIRS)
target_link_libraries(ged-facetize ${FACETIZE_LIBS})
set_property(TARGET ged-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
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
