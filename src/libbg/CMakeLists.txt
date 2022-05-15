# Include directories needed by libbg users
set(BG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BN_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${POLY2TRI_INCLUDE_DIRS}
  ${SPSR_INCLUDE_DIR}
  ${LMDB_INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/other/ext/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(bg BG_INCLUDE_DIRS "")

set(SPSR_srcs
  spsr/Allocator.h
  spsr/Array.h
  spsr/Array.inl
  spsr/BinaryNode.h
  spsr/BSplineData.h
  spsr/BSplineData.inl
  spsr/cvertex.h
  spsr/Factor.cpp
  spsr/Factor.h
  spsr/FunctionData.h
  spsr/FunctionData.inl
  spsr/Geometry.cpp
  spsr/Geometry.h
  spsr/Geometry.inl
  spsr/MarchingCubes.cpp
  spsr/MarchingCubes.h
  spsr/MAT.h
  spsr/MAT.inl
  spsr/MultiGridOctreeData.h
  spsr/MultiGridOctreeData.inl
  spsr/MultiGridOctreeData.IsoSurface.inl
  spsr/MultiGridOctreeData.SortedTreeNodes.inl
  spsr/Octree.h
  spsr/Octree.inl
  spsr/PlyVertexMini.h
  spsr/PointStream.h
  spsr/PointStream.inl
  spsr/Polynomial.h
  spsr/Polynomial.inl
  spsr/PPolynomial.h
  spsr/PPolynomial.inl
  spsr/SparseMatrix.h
  spsr/SparseMatrix.inl
  spsr/SPSR.cpp
  spsr/SPSR.h
  spsr/Vector.h
  spsr/Vector.inl
  )

set(LIBBG_SOURCES
  aabb_ray.c
  chull.c
  chull2.cpp
  QuickHull.cpp
  chull3d.cpp
  clip.c
  clipper.cpp
  lod.cpp
  lseg_lseg.c
  lseg_pt.c
  obr.c
  plane.cpp
  polygon.c
  polygon_bview.c
  polygon_fill.cpp
  polygon_op.cpp
  polygon_triangulate.cpp
  polygon_point_in.c
  sat.c
  snap.c
  spsr.c
  ${SPSR_srcs}
  tri_pt.c
  tri_ray.c
  tri_tri.c
  trimesh.cpp
  trimesh_isect.cpp
  trimesh_sync.cpp
  trimesh_split.cpp
  vert_tree.c
  util.c
  )

BRLCAD_ADDLIB(libbg "${LIBBG_SOURCES}" "libbv;libbn;libbu;${POLY2TRI_LIBRARIES};${LMDB_LIBRARIES}")
set_target_properties(libbg PROPERTIES VERSION 20.0.1 SOVERSION 20)
if (HIDE_INTERNAL_SYMBOLS)
  if (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
    set_property(TARGET libbg APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_IMPORTS")
  endif (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
  if (TARGET libbg-obj)
    if (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
      set_property(TARGET libbg-obj APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_IMPORTS")
    endif (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
  endif (TARGET libbg-obj)
endif (HIDE_INTERNAL_SYMBOLS)

# altivec vectorization is broken in our bundled version of eigen.
# looks like it was fixed in upstream long time ago, so just disable
# for now.  TODO: remove this / retest after next Eigen upgrade.
set_property(TARGET libbg APPEND PROPERTY COMPILE_DEFINITIONS "EIGEN_DONT_VECTORIZE")
set_property(TARGET libbg-obj APPEND PROPERTY COMPILE_DEFINITIONS "EIGEN_DONT_VECTORIZE")

add_subdirectory(tests)

set(bg_ignore
  QuickHull.hpp
  bg_private.h
  clipper.hpp
  delaunator.hpp
  earcut.hpp
  pointgen.c
  spsr/LICENSE
  )

CMAKEFILES(${bg_ignore})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

