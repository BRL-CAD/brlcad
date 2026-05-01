# Geogram subset for BRL-CAD
# Compiled as part of libbg (not as a separate library).
# Minimal copy supporting the bot remesh command: GEO::initialize, mesh_repair
# (used internally by CVT/RVD and as pre-processing in libged/bot/remesh.cpp),
# compute_normals, remesh_smooth
# Extracted from geogram/src/lib/geogram/

find_package_gte(REQUIRED)

# Collect geogram source files as absolute paths so the parent CMakeLists.txt
# can include them directly into libbg.
set(GEOGRAM_SOURCES
    # basic infrastructure
    basic/assert.cpp
    basic/attributes.cpp
    basic/geogram_options.cpp
    basic/geometry.cpp
    basic/numeric.cpp
    basic/packed_arrays.cpp
    basic/parallel.cpp

    # numerics
    numerics/PCK.cpp
    numerics/multi_precision.cpp
    numerics/optimizer.cpp
    numerics/predicates.cpp
    # delaunay (needed for CVT remeshing and LFS)
    delaunay/delaunay.cpp
    delaunay/delaunay_nn.cpp
    # mesh
    mesh/mesh.cpp
    mesh/mesh_AABB.cpp
    mesh/mesh_geometry.cpp
    mesh/mesh_halfedges.cpp
    mesh/mesh_partition.cpp
    mesh/mesh_preprocessing.cpp
    mesh/mesh_remesh.cpp
    mesh/mesh_reorder.cpp
    mesh/mesh_repair.cpp
    mesh/mesh_topology.cpp
    # voronoi (needed for remesh_smooth CVT)
    voronoi/CVT.cpp
    voronoi/RVD.cpp
    voronoi/RVD_callback.cpp
    voronoi/RVD_mesh_builder.cpp
    voronoi/generic_RVD_cell.cpp
    voronoi/generic_RVD_polygon.cpp
    voronoi/integration_simplex.cpp
    # points (needed for colocate in mesh_repair used by CVT/remesh pre-processing)
    points/colocate.cpp
    points/kd_tree.cpp
    points/nn_search.cpp
    # OpenNL linear solver (needed for remesh CVT Newton step)
    NL/nl_amgcl.cpp
    NL/nl_api.c
    NL/nl_blas.c
    NL/nl_context.c
    NL/nl_iterative_solvers.c
    NL/nl_matrix.c
    NL/nl_os.c
    NL/nl_preconditioners.c
)

set(GEOGRAM_SRCS)
foreach(gs ${GEOGRAM_SOURCES})
  list(APPEND GEOGRAM_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/${gs}")
endforeach()

# All geogram sources need BRLCAD_GEOGRAM_EMBED and NL_WITH_AMGCL.
# NOTE: set_source_files_properties is scoped to the directory where it is
# called.  The actual property assignments for these files happen in the
# parent (libbg) CMakeLists.txt after add_subdirectory(geogram), using the
# GEOGRAM_SRCS variable exported to PARENT_SCOPE below.

# Export these variables to the parent scope so libbg/CMakeLists.txt can use them
set(GEOGRAM_SRCS ${GEOGRAM_SRCS} PARENT_SCOPE)
set(GEOGRAM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. PARENT_SCOPE)
set(GEOGRAM_AMGCL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/amgcl PARENT_SCOPE)
set(GEOGRAM_AMGCL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_amgcl.cpp PARENT_SCOPE)

set(GEOGRAM_OTHER_SRCS
  NL/nl.h
  NL/nl_64.h
  NL/nl_amgcl.h
  NL/nl_blas.h
  NL/nl_context.h
  NL/nl_iterative_solvers.h
  NL/nl_matrix.h
  NL/nl_preconditioners.h
  NL/nl_private.h
  basic/algorithm.cpp
  basic/algorithm.h
  basic/argused.h
  basic/assert.h
  basic/attributes.h
  basic/geogram_common.h
  basic/geogram_options.h
  basic/geometry.h
  basic/geometry_nd.h
  basic/memory.h
  basic/numeric.h
  basic/packed_arrays.h
  basic/permutation.h
  basic/rationalg.h
  basic/thread_sync.h
  basic/vecg.h
  basic/vechg.h
  basic/vector_attribute.h
  delaunay/delaunay.h
  delaunay/delaunay_nn.h
  mesh/index.h
  mesh/mesh.h
  mesh/mesh_AABB.h
  mesh/mesh_geometry.h
  mesh/mesh_halfedges.h
  mesh/mesh_partition.h
  mesh/mesh_preprocessing.h
  mesh/mesh_remesh.h
  mesh/mesh_reorder.h
  mesh/mesh_repair.h
  mesh/mesh_sampling.h
  mesh/mesh_topology.h
  numerics/PCK.h
  numerics/multi_precision.h
  numerics/optimizer.h
  numerics/predicates.h
  numerics/predicates/aligned3d.h
  numerics/predicates/det3d.h
  numerics/predicates/det4d.h
  numerics/predicates/det_compare_4d.h
  numerics/predicates/dot3d.h
  numerics/predicates/dot_compare_3d.h
  numerics/predicates/orient2d.h
  numerics/predicates/orient3d.h
  numerics/predicates/side1.h
  numerics/predicates/side2.h
  numerics/predicates/side3.h
  numerics/predicates/side3_2dlifted.h
  numerics/predicates/side3h.h
  numerics/predicates/side4.h
  numerics/predicates/side4h.h
  points/colocate.h
  points/kd_tree.h
  points/nn_search.h
  third_party/amgcl/CMakeLists.txt
  third_party/amgcl/LICENSE.md
  third_party/amgcl/README.md
  third_party/amgcl/amgcl/adapter/zero_copy.hpp
  third_party/amgcl/amgcl/amg.hpp
  third_party/amgcl/amgcl/backend/builtin.hpp
  third_party/amgcl/amgcl/backend/detail/matrix_ops.hpp
  third_party/amgcl/amgcl/backend/interface.hpp
  third_party/amgcl/amgcl/coarsening/detail/galerkin.hpp
  third_party/amgcl/amgcl/coarsening/plain_aggregates.hpp
  third_party/amgcl/amgcl/coarsening/pointwise_aggregates.hpp
  third_party/amgcl/amgcl/coarsening/smoothed_aggr_emin.hpp
  third_party/amgcl/amgcl/coarsening/smoothed_aggregation.hpp
  third_party/amgcl/amgcl/coarsening/tentative_prolongation.hpp
  third_party/amgcl/amgcl/detail/inverse.hpp
  third_party/amgcl/amgcl/detail/qr.hpp
  third_party/amgcl/amgcl/detail/sort_row.hpp
  third_party/amgcl/amgcl/detail/spgemm.hpp
  third_party/amgcl/amgcl/make_solver.hpp
  third_party/amgcl/amgcl/preconditioner/dummy.hpp
  third_party/amgcl/amgcl/relaxation/spai0.hpp
  third_party/amgcl/amgcl/reorder/cuthill_mckee.hpp
  third_party/amgcl/amgcl/solver/cg.hpp
  third_party/amgcl/amgcl/solver/detail/default_inner_product.hpp
  third_party/amgcl/amgcl/solver/skyline_lu.hpp
  third_party/amgcl/amgcl/util.hpp
  third_party/amgcl/amgcl/value_type/interface.hpp
  third_party/liblbfgs/AI_Discussion.txt
  third_party/liblbfgs/CMakeLists.txt
  third_party/liblbfgs/COPYING
  third_party/liblbfgs/README
  third_party/liblbfgs/fortran/Readme
  third_party/liblbfgs/fortran/lbfgs.f
  third_party/liblbfgs/fortran/makefile
  third_party/liblbfgs/fortran/sdrive.f
  third_party/liblbfgs/lbfgs.h
  third_party/liblbfgs/lib/arithmetic_ansi.h
  third_party/liblbfgs/lib/arithmetic_sse_double.h
  third_party/liblbfgs/lib/arithmetic_sse_float.h
  third_party/liblbfgs/lib/lbfgs.c
  third_party/liblbfgs/sample/sample.c
  third_party/liblbfgs/sample/sample.cpp
  voronoi/CVT.h
  voronoi/RVD.h
  voronoi/RVD_callback.h
  voronoi/RVD_mesh_builder.h
  voronoi/generic_RVD.h
  voronoi/generic_RVD_cell.h
  voronoi/generic_RVD_polygon.h
  voronoi/generic_RVD_utils.h
  voronoi/generic_RVD_vertex.h
  voronoi/integration_simplex.h
)

cmakefiles(
  CMakeLists.txt
  ${GEOGRAM_SOURCES}
  ${GEOGRAM_OTHER_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
