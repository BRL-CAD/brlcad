add_subdirectory(geogram)

# Apply geogram-specific compile flags and definitions.
# These must be set in the parent scope (libbg) since set_source_files_properties
# is scoped to the directory where it is called and is not inherited by parents.
#
# -frounding-math and -ffp-contract=off are required to preserve the correctness
# of exact-arithmetic predicates.  -Wno-float-equal suppresses comparisons that
# are intrinsic to geogram's exact arithmetic.  The other suppressed warnings
# reflect code patterns in this third-party subset that do not indicate defects.
foreach(_src ${GEOGRAM_SRCS})
    set_source_files_properties(${_src} PROPERTIES
        COMPILE_DEFINITIONS "BRLCAD_GEOGRAM_EMBED;NL_WITH_AMGCL"
    )
endforeach()
if(NOT MSVC)
    set(_geo_flags
        -frounding-math
        -ffp-contract=off
        -Wno-float-equal
        -Wno-shadow
        -Wno-unused-parameter
    )
    foreach(_src ${GEOGRAM_SRCS})
        set_source_files_properties(${_src} PROPERTIES
            COMPILE_OPTIONS "${_geo_flags}"
        )
    endforeach()
    # nl_amgcl.cpp includes AMGCL headers that use #pragma omp; suppress the
    # resulting -Wunknown-pragmas diagnostic (OpenMP is not required here).
    set_source_files_properties(${GEOGRAM_AMGCL_SRC} PROPERTIES
        COMPILE_OPTIONS "${_geo_flags};-Wno-unknown-pragmas"
    )
    unset(_geo_flags)
else()
    foreach(_src ${GEOGRAM_SRCS})
        set_source_files_properties(${_src} PROPERTIES
            COMPILE_OPTIONS "/fp:precise"
        )
    endforeach()
endif()

find_package_eigen(REQUIRED)
find_package_gte(REQUIRED)
find_package_spsr(REQUIRED)

set(
  LIBBG_SOURCES
  aabb_ray.c
  ballpivot.cpp
  chull.c
  chull2.cpp
  QuickHull.cpp
  chull3d.cpp
  clip.c
  clipper.cpp
  decimate.cpp
  lseg_lseg.cpp
  lseg_pt.cpp
  obr.cpp
  pca.cpp
  plane.cpp
  polygon.c
  polygon_op.cpp
  polygon_triangulate.cpp
  polygon_point_in.c
  sat.cpp
  spsr.cpp
  tri_pt.cpp
  tri_ray.c
  tri_tri.c
  trimesh.cpp
  trimesh_diff.cpp
  trimesh_isect.cpp
  trimesh_plot3.cpp
  trimesh_repair.cpp
  trimesh_remesh.cpp
  trimesh_sync.cpp
  trimesh_split.cpp
  vert_tree.c
  util.c
  # Geogram sources compiled as part of libbg
  ${GEOGRAM_SRCS}
)

# Note - libbg_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(BG_LIBS ${libbg_deps})
if(UNIX)
  list(APPEND BG_LIBS pthread dl)
endif()

set(BG_INCLUDE_DIRS
  ${LMDB_INCLUDE_DIR}
  ${EIGEN3_INCLUDE_DIR}
  # The in-tree GTE directory must come before the external GTE_INCLUDE_DIR so
  # that BRL-CAD's custom GTE headers (MeshRepair.h, MeshHoleFilling.h, etc.)
  # shadow any identically-named files in the external GTE install.
  ${CMAKE_CURRENT_SOURCE_DIR}/GTE
  ${GTE_INCLUDE_DIR}
  ${SPSR_INCLUDE_DIR}
  # Geogram internal include dirs (not installed as public API)
  ${GEOGRAM_INCLUDE_DIR}
  ${GEOGRAM_AMGCL_INCLUDE_DIR}
  )

brlcad_addlib(libbg "${LIBBG_SOURCES}" "${BG_LIBS}" "" "${BG_INCLUDE_DIRS}"
  # These files define macros that conflict when batched into the same unity TU:
  # - polygon.c, polygon_triangulate.cpp, trimesh_isect.cpp, trimesh_plot3.cpp:
  #   all define PLOT_PREFIX_STR to remap plot3 function names, causing
  #   -Werror redefinition errors and unresolved bg_plot3_* symbols.
  # - geogram files need special rounding and fp-contract flags that must
  #   not affect the surrounding libbg unity TU.
  UNITY_BUILD_SKIP
    polygon.c
    polygon_triangulate.cpp
    spsr.cpp
    trimesh_isect.cpp
    trimesh_plot3.cpp
    ${GEOGRAM_SRCS}
)
set_target_properties(libbg PROPERTIES VERSION 20.0.1 SOVERSION 20)

# enable /bigobj for spsr.cpp if we can
# spsr has a lot of templates and inlines that will exceed (MSVC in particular)
# default section limit without /bigobj
check_cxx_compiler_flag("/bigobj" CXX_BIGOBJ_SUPPORTED)
if (CXX_BIGOBJ_SUPPORTED)
  set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/spsr.cpp
    PROPERTIES COMPILE_OPTIONS "/bigobj"
  )
endif()

# altivec vectorization is broken in our bundled version of eigen.
# looks like it was fixed in upstream long time ago, so just disable
# for now.  TODO: remove this / retest after next Eigen upgrade.
set_property(TARGET libbg APPEND PROPERTY COMPILE_DEFINITIONS "EIGEN_DONT_VECTORIZE")
if(TARGET libbg-obj)
  set_property(TARGET libbg-obj APPEND PROPERTY COMPILE_DEFINITIONS "EIGEN_DONT_VECTORIZE")
endif(TARGET libbg-obj)

add_subdirectory(tests)

set(
  bg_ignore
  BallPivot.hpp
  QuickHull.hpp
  bg_private.h
  clipper.hpp
  delaunator.hpp
  detria.hpp
  earcut.hpp
  nanoflann.hpp
  pointgen.c
  predicates.h
  GTE/Mathematics/BotSmooth.h
  GTE/Mathematics/BotSubdivide.h
  GTE/Mathematics/TriMesh.h
  GTE/Mathematics/LSCMParameterization.h
  GTE/Mathematics/MeshHoleFilling.h
  GTE/Mathematics/MeshPreprocessing.h
  GTE/Mathematics/MeshQuality.h
  GTE/Mathematics/MeshRepair.h
  GTE/Mathematics/MeshValidation.h
)

cmakefiles(${bg_ignore})
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
