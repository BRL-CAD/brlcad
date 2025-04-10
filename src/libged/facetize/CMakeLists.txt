brlcad_find_package(manifold REQUIRED)

set(FACETIZE_LOCAL_INCLUDE_DIRS)

set(FACETIZE_LIBS libged libbu manifold::manifold)

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

ged_plugin_library(ged-facetize ${FACETIZE_SRCS})
target_include_directories(ged-facetize BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FACETIZE_LOCAL_INCLUDE_DIRS)
  target_include_directories(ged-facetize SYSTEM BEFORE PRIVATE ${FACETIZE_LOCAL_INCLUDE_DIRS})
endif(FACETIZE_LOCAL_INCLUDE_DIRS)
target_link_libraries(ged-facetize ${FACETIZE_LIBS})

# Plugin requires subprocess to work
add_dependencies(ged-facetize ged_exec facetize_process)

# Old version of facetize
set(OLD_FACETIZE_SRCS old_facetize.cpp old_facetize_log.c)
ged_plugin_library(ged-old-facetize ${OLD_FACETIZE_SRCS})
target_include_directories(ged-old-facetize BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(FACETIZE_LOCAL_INCLUDE_DIRS)
  target_include_directories(ged-old-facetize SYSTEM BEFORE PRIVATE ${FACETIZE_LOCAL_INCLUDE_DIRS})
endif(FACETIZE_LOCAL_INCLUDE_DIRS)
target_link_libraries(ged-old-facetize ${FACETIZE_LIBS})

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
