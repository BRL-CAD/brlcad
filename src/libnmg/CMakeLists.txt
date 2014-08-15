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
  globals.c
  nurb_trim.c
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
  uvpoints.cpp
  )
CMAKEFILES(${libnmg_ignored_files})

BRLCAD_ADDLIB(libnmg "${LIBNMG_SOURCES}" "${GDIAM_LIBRARY};${VDS_LIBRARY};libbn;libbu;libnurb;${OPENNURBS_LIBRARY};${P2T_LIBRARY};libbrep;${REGEX_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}" NO_STRICT_CXX)

SET_TARGET_PROPERTIES(libnmg PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "NMG_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_IMPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_IMPORTS")
  SET_PROPERTY(TARGET libnmg APPEND PROPERTY COMPILE_DEFINITIONS "NURB_DLL_IMPORTS")
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
