# Include directories needed by libnurbs users
set(NURBS_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
  )

# locally used but not needed by users of the library
set(NURBS_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIR}
  ${TNT_INCLUDE_DIR}
  #${BRLCAD_SOURCE_DIR}/src/other/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(nurbs NURBS_INCLUDE_DIRS NURBS_LOCAL_INCLUDE_DIRS)

set(LIBNURBS_SOURCES
  opennurbs_ext.cpp
  #opennurbs_fit.cpp
  PullbackCurve.cpp
  )

set(librt_ignored_files
  opennurbs_fit.h
  opennurbs_fit.cpp
  PullbackCurve.h
  )
CMAKEFILES(${librt_ignored_files})

# Initialize librt_DEFINES in case of reconfiguration
set(libnurbs_DEFINES "")

# Set libnurbs compile definitions
get_property(libnurbs_DEFINES GLOBAL PROPERTY libnurbs_DEFINES)
list(APPEND libnurbs_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  list(APPEND libnurbs_DEFINES "ON_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
set_property(GLOBAL PROPERTY libnurbs_DEFINES "${libnurbs_DEFINES}")

BRLCAD_ADDLIB(libnurbs "${LIBNURBS_SOURCES}" "libbn;libbu;${OPENNURBS_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}" NOSTRICTCXX)
#set_property(TARGET libnurbs APPEND PROPERTY COMPILE_FLAGS "-Wno-shadow -Wno-float-equal")

if(BUILD_STATIC_LIBS)
  set_property(TARGET libnurbs-static APPEND PROPERTY COMPILE_DEFINITIONS "OBJ_BREP=1")
endif(BUILD_STATIC_LIBS)

set_target_properties(libnurbs PROPERTIES VERSION 20.0.1 SOVERSION 20)
if(CPP_DLL_DEFINES)
  if(BRLCAD_BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
    SET_PROPERTY(TARGET libnurbs-static APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
  endif(BRLCAD_BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
endif(CPP_DLL_DEFINES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
