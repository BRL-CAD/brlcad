brlcad_find_package(manifold REQUIRED)

set(FACETIZE_LIBS libged libbu manifold::manifold)

add_subdirectory(subprocess)
add_subdirectory(tests)

set(
  FACETIZE_SRCS
  brep.cpp
  facetize.cpp
  nmg.cpp
  orchestrator.cpp
  plan.cpp
  process.cpp
  regions.cpp
  reuse.cpp
  transfer.cpp
  tri.cpp
  util.cpp
  validation.cpp
  worker.cpp
)

ged_plugin_library(ged-facetize
  ${FACETIZE_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${FACETIZE_LIBS}
  PLUGIN_ONLY
)

# Plugin requires subprocess to work
add_dependencies(ged-facetize ged_exec facetize_process)

cmakefiles(
  CMakeLists.txt
  tess_opts.h
  ged_facetize.h
  orchestrator.h
  process.h
  reuse.h
  transfer.h
  validation.h
  worker.h
  TODO
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
