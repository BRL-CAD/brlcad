# Include directories needed by libnmg users
set(NMG_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${BREP_INCLUDE_DIRS}
  ${P2T_INCLUDE_DIR}
  ${OPENNURBS_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  )

# locally used but not needed by users of the library
set(NMG_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIR}
  ${VDS_INCLUDE_DIR}
  ${GDIAM_INCLUDE_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(nmg NMG_INCLUDE_DIRS NMG_LOCAL_INCLUDE_DIRS)

set(LIBNMG_SOURCES
  dspline.c
  globals.c
  oslo_calc.c
  oslo_map.c
  primitives/bspline/nurb_basis.c
  primitives/bspline/nurb_bezier.c
  primitives/bspline/nurb_bound.c
  primitives/bspline/nurb_c2.c
  primitives/bspline/nurb_copy.c
  primitives/bspline/nurb_diff.c
  primitives/bspline/nurb_eval.c
  primitives/bspline/nurb_flat.c
  primitives/bspline/nurb_interp.c
  primitives/bspline/nurb_knot.c
  primitives/bspline/nurb_norm.c
  primitives/bspline/nurb_plot.c
  primitives/bspline/nurb_poly.c
  primitives/bspline/nurb_ray.c
  primitives/bspline/nurb_refine.c
  primitives/bspline/nurb_reverse.c
  primitives/bspline/nurb_solve.c
  primitives/bspline/nurb_split.c
  primitives/bspline/nurb_trim.c
  primitives/bspline/nurb_trim_util.c
  primitives/bspline/nurb_util.c
  primitives/bspline/nurb_xsplit.c
  primitives/nmg/nmg_ck.c
  primitives/nmg/nmg_class.c
  primitives/nmg/nmg_copy.c
  primitives/nmg/nmg_eval.c
  primitives/nmg/nmg_extrude.c
  primitives/nmg/nmg_fcut.c
  primitives/nmg/nmg_fuse.c
  primitives/nmg/nmg_index.c
  primitives/nmg/nmg_info.c
  primitives/nmg/nmg_inter.c
  primitives/nmg/nmg_rt_isect.c
  primitives/nmg/nmg_rt_segs.c
  primitives/nmg/nmg_manif.c
  primitives/nmg/nmg_mesh.c
  primitives/nmg/nmg_misc.c
  primitives/nmg/nmg_mk.c
  primitives/nmg/nmg_mod.c
  primitives/nmg/nmg_plot.c
  primitives/nmg/nmg_pr.c
  primitives/nmg/nmg_pt_fu.c
  primitives/nmg/nmg_tri.c
  primitives/nmg/nmg_visit.c
  vlist.c
  )

set(libnmg_ignored_files
  columnparse.c
  ../librt/librt_private.h
  namegen.c
  prcomb.c
  primitives/nmg/nmg_junk.c
  timer-nt.c
  timerhep.c
  timerunix.c
  uvpoints.cpp
  )
CMAKEFILES(${libnmg_ignored_files})

if(MSVC)
  set(LIBNMG_SOURCES ${LIBNMG_SOURCES} timer-nt.c)
else(MSVC)
  set(LIBNMG_SOURCES ${LIBNMG_SOURCES} timer42.c)
endif(MSVC)

BRLCAD_ADDLIB(libnmg "${LIBNMG_SOURCES}" "${GDIAM_LIBRARY};${VDS_LIBRARY};libbn;libbu;${OPENNURBS_LIBRARY};${P2T_LIBRARY};libbrep;${REGEX_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}" NO_STRICT_CXX)

SET_TARGET_PROPERTIES(libnmg PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_IMPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)


include_directories(
  ${CMAKE_SOURCE_DIR}/src/other/Eigen
  ${CMAKE_CURRENT_SOURCE_DIR}/../libbrep
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
