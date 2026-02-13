brlcad_find_package(manifold REQUIRED)

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

ged_plugin_library(ged-facetize
  ${FACETIZE_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${FACETIZE_LIBS}
  PLUGIN_ONLY
)

# Plugin requires subprocess to work
add_dependencies(ged-facetize ged_exec facetize_process)

# Old version of facetize
set(OLD_FACETIZE_SRCS
  old_facetize.cpp
  old_facetize_log.c
)

ged_plugin_library(ged-old_facetize
  ${OLD_FACETIZE_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${FACETIZE_LIBS}
)

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
