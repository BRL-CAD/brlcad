# Include directories needed by libbrep users
set(BREP_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  )

# locally used but not needed by users of the library
set(BREP_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIR}
  ${TNT_INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/other/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(brep BREP_INCLUDE_DIRS BREP_LOCAL_INCLUDE_DIRS)

set(LIBBREP_SOURCES
  intersect.cpp
  opennurbs_ext.cpp
  #opennurbs_fit.cpp
  PullbackCurve.cpp
  ssx_event.cpp
  )

set(libbrep_ignored_files
  opennurbs_fit.h
  opennurbs_fit.cpp
  PullbackCurve.h
  )
CMAKEFILES(${libbrep_ignored_files})
CMAKEFILES(Makefile.am)

# Initialize libbrep_DEFINES in case of reconfiguration
set(libbrep_DEFINES "")

# Set libbrep compile definitions
get_property(libbrep_DEFINES GLOBAL PROPERTY libbrep_DEFINES)
list(APPEND libbrep_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  list(APPEND libbrep_DEFINES "ON_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
set_property(GLOBAL PROPERTY libbrep_DEFINES "${libbrep_DEFINES}")

BRLCAD_ADDLIB(libbrep "${LIBBREP_SOURCES}" "libbn;libbu;${OPENNURBS_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}" NOSTRICTCXX)

if(BUILD_STATIC_LIBS)
  set_property(TARGET libbrep-static APPEND PROPERTY COMPILE_DEFINITIONS "OBJ_BREP=1")
endif(BUILD_STATIC_LIBS)

set_target_properties(libbrep PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(CPP_DLL_DEFINES)
  if(BRLCAD_BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
    SET_PROPERTY(TARGET libbrep-static APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
  endif(BRLCAD_BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
endif(CPP_DLL_DEFINES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
