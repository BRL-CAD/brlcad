# Include directories needed by libbg users
set(BG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BN_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${P2T_INCLUDE_DIR}
  )
BRLCAD_LIB_INCLUDE_DIRS(bg BG_INCLUDE_DIRS "")

set(LIBBG_SOURCES
  aabb_ray.c
  chull.c
  chull2.cpp
  QuickHull.cpp
  chull3d.cpp
  clipper.cpp
  lseg_lseg.c
  obr.c
  polygon.c
  polygon_triangulate.cpp
  polygon_point_in.c
  spsr.c
  tri_pt.c
  tri_ray.c
  tri_tri.c
  trimesh.cpp
  trimesh_isect.cpp
  trimesh_pt_in.c
  util.c
  )

BRLCAD_ADDLIB(libbg "${LIBBG_SOURCES}" "libbn;libbu;SPSR;${P2T_LIBRARY}")
set_target_properties(libbg PROPERTIES VERSION 20.0.1 SOVERSION 20)
if (HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libbg APPEND PROPERTY COMPILE_DEFINITIONS "SPSR_DLL_IMPORTS")
  if (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
    set_property(TARGET libbg APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_IMPORTS")
  endif (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
  if (TARGET libbg-obj)
    set_property(TARGET libbg-obj APPEND PROPERTY COMPILE_DEFINITIONS "SPSR_DLL_IMPORTS")
    if (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
      set_property(TARGET libbg-obj APPEND PROPERTY COMPILE_DEFINITIONS "P2T_DLL_IMPORTS")
    endif (TARGET poly2tri OR HIDE_INTERNAL_SYMBOLS_EXT)
  endif (TARGET libbg-obj)
endif (HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

set(bg_ignore
  QuickHull.hpp
  bg_private.h
  clipper.hpp
  delaunator.hpp
  earcut.hpp
  pointgen.c
  )

CMAKEFILES(${bg_ignore})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
