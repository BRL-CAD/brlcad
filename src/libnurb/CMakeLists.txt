# Include directories needed by libnurb users
set(NURB_INCLUDE_DIRS
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
set(NURB_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIR}
  ${VDS_INCLUDE_DIR}
  ${GDIAM_INCLUDE_DIR}
  )

BRLCAD_LIB_INCLUDE_DIRS(nurb NURB_INCLUDE_DIRS NURB_LOCAL_INCLUDE_DIRS)

set(LIBNURB_SOURCES
  basis.c
  bezier.c
  bound.c
  c2.c
  copy.c
  diff.c
  eval.c
  flat.c
  interp.c
  knot.c
  norm.c
  oslo_calc.c
  oslo_map.c
  plot.c
  poly.c
  ray.c
  refine.c
  reverse.c
  solve.c
  split.c
  trim.c
  trim_util.c
  util.c
  xsplit.c
  )

set(libnurb_ignored_files
  )
CMAKEFILES(${libnurb_ignored_files})

BRLCAD_ADDDATA(nurb_example.c sample_applications)

BRLCAD_ADDLIB(libnurb "${LIBNURB_SOURCES}" "${GDIAM_LIBRARY};${VDS_LIBRARY};libbn;libbu;${OPENNURBS_LIBRARY};${P2T_LIBRARY};libbrep;${REGEX_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}" NO_STRICT_CXX)

SET_TARGET_PROPERTIES(libnurb PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET libnurb APPEND PROPERTY COMPILE_DEFINITIONS "TIE_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnurb APPEND PROPERTY COMPILE_DEFINITIONS "DB5_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnurb APPEND PROPERTY COMPILE_DEFINITIONS "NURB_DLL_EXPORTS")
  SET_PROPERTY(TARGET libnurb APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_IMPORTS")
  SET_PROPERTY(TARGET libnurb APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_IMPORTS")
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
