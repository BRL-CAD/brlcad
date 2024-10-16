brlcad_find_package(manifold REQUIRED)
brlcad_find_package(pugixml)
brlcad_find_package(assimp)

set(FACETIZE_LOCAL_INCLUDE_DIRS)

if(TARGET manifold::manifold)
  add_definitions(-DUSE_MANIFOLD)

  if(TARGET assimp::assimp AND TARGET pugixml::pugixml)
    set(ASSETIMPORT_LIBRARIES assimp::assimp pugixml::pugixml)
    add_definitions(-DUSE_ASSETIMPORT)
  endif(TARGET assimp::assimp AND TARGET pugixml::pugixml)
endif()

set(FACETIZE_LIBS libged libbu manifold::manifold ${ASSETIMPORT_LIBRARIES})

add_subdirectory(subprocess)

set(
  FACETIZE_SRCS
  brep.cpp
  facetize.cpp
  nmg.cpp
  regions.cpp
  tri.cpp
  util.cpp
)

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-facetize SHARED ${FACETIZE_SRCS})
target_include_directories(ged-facetize BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FACETIZE_LOCAL_INCLUDE_DIRS)
  target_include_directories(ged-facetize SYSTEM BEFORE PRIVATE ${FACETIZE_LOCAL_INCLUDE_DIRS})
endif(FACETIZE_LOCAL_INCLUDE_DIRS)
target_link_libraries(ged-facetize ${FACETIZE_LIBS})
set_property(TARGET ged-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
validate_style(ged-facetize facetize.cpp)
plugin_setup(ged-facetize ged)

# Plugin requires subprocess to work
add_dependencies(ged-facetize ged_exec facetize_process)

# Old version of facetize
set(OLD_FACETIZE_SRCS old_facetize.cpp old_facetize_log.c)
ged_plugin_library(ged-old-facetize SHARED ${OLD_FACETIZE_SRCS})
target_include_directories(ged-old-facetize BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FACETIZE_LOCAL_INCLUDE_DIRS)
  target_include_directories(ged-old-facetize SYSTEM BEFORE PRIVATE ${FACETIZE_LOCAL_INCLUDE_DIRS})
endif(FACETIZE_LOCAL_INCLUDE_DIRS)
target_link_libraries(ged-old-facetize ${FACETIZE_LIBS})
set_property(TARGET ged-old-facetize APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
plugin_setup(ged-old-facetize ged)

cmakefiles(
  CMakeLists.txt
  tess_opts.h
  ged_facetize.h
  old_facetize.h
  subprocess.h
  TODO
  ${FACETIZE_SRCS}
  ${OLD_FACETIZE_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
