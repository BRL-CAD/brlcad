set(
  GED_TESS_SRCS
  brep_csg.cpp
  co3ne.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

brlcad_find_package(manifold REQUIRED)
brlcad_find_package(Geogram REQUIRED SYSPATTERN geogram)
set(INCLUDE_DIRS ${Geogram_INCLUDE_DIR})

ged_subprocess_library(facetize_process SHARED ${GED_TESS_SRCS})
target_include_directories(facetize_process SYSTEM PRIVATE ${INCLUDE_DIRS})
target_link_libraries(facetize_process libged;manifold::manifold;${Geogram_LIBRARY})
set_property(TARGET facetize_process APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H MANIFOLD_DLL_IMPORTS)
validate_style(facetize_process ${GED_TESS_SRCS})
plugin_setup(facetize_process ged_exec)

cmakefiles(
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
