# Geogram subset for BRL-CAD
# Compiled as part of libbg (not as a separate library).
# Minimal copy supporting the bot remesh command: GEO::initialize, mesh_repair
# (used internally by CVT/RVD and as pre-processing in libged/bot/remesh.cpp),
# compute_normals, remesh_smooth
# Extracted from geogram/src/lib/geogram/

find_package_gte(REQUIRED)

# Collect geogram source files as absolute paths so the parent CMakeLists.txt
# can include them directly into libbg.
set(GEOGRAM_SRCS
    # basic infrastructure
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/assert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/attributes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/geogram_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/numeric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/packed_arrays.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/basic/parallel.cpp

    # numerics
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/PCK.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/multi_precision.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/optimizer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/numerics/predicates.cpp
    # delaunay (needed for CVT remeshing and LFS)
    ${CMAKE_CURRENT_SOURCE_DIR}/delaunay/delaunay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/delaunay/delaunay_nn.cpp
    # mesh
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_AABB.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_halfedges.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_partition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_preprocessing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_remesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_reorder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_repair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mesh/mesh_topology.cpp
    # voronoi (needed for remesh_smooth CVT)
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/CVT.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/RVD.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/RVD_callback.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/RVD_mesh_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/generic_RVD_cell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/generic_RVD_polygon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/voronoi/integration_simplex.cpp
    # points (needed for colocate in mesh_repair used by CVT/remesh pre-processing)
    ${CMAKE_CURRENT_SOURCE_DIR}/points/colocate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/points/kd_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/points/nn_search.cpp
    # OpenNL linear solver (needed for remesh CVT Newton step)
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_amgcl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_blas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_iterative_solvers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_os.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NL/nl_preconditioners.c
)

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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
