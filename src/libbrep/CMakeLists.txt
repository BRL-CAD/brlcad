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
  ${BRLCAD_SOURCE_DIR}/src/other/Eigen
  )

BRLCAD_LIB_INCLUDE_DIRS(brep BREP_INCLUDE_DIRS BREP_LOCAL_INCLUDE_DIRS)

set(LIBBREP_SOURCES
  BBNode.cpp
  boolean.cpp
  BRNode.cpp
  intersect.cpp
  libbrep_brep_tools.cpp
  opennurbs_ext.cpp
  #opennurbs_fit.cpp
  px_event.cpp
  PullbackCurve.cpp
  shape_recognition.cpp
  shape_recognition_cone.cpp
  shape_recognition_cylinder.cpp
  shape_recognition_sphere.cpp
  shape_recognition_pipeline.cpp
  shape_recognition_planar.cpp
  shape_recognition_util.cpp
  ssx_event.cpp
  Subcurve.cpp
  Subsurface.cpp
  )

set(libbrep_ignored_files
  libbrep_brep_tools.h
  brep_except.h
  opennurbs_fit.h
  opennurbs_fit.cpp
  PullbackCurve.h
  shape_recognition.h
  shape_recognition_torus.cpp
  surface_tree_queue_tests.patch
  )
CMAKEFILES(${libbrep_ignored_files})

# Initialize libbrep_DEFINES in case of reconfiguration
set(libbrep_DEFINES "")

# Set libbrep compile definitions
get_property(libbrep_DEFINES GLOBAL PROPERTY libbrep_DEFINES)
list(APPEND libbrep_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  list(APPEND libbrep_DLL_DEFINES "ON_DLL_IMPORTS")
  set_property(GLOBAL PROPERTY libbrep_DLL_DEFINES "${libbrep_DLL_DEFINES}")
endif(CPP_DLL_DEFINES)
set_property(GLOBAL PROPERTY libbrep_DEFINES "${libbrep_DEFINES}")

BRLCAD_ADDLIB(libbrep "${LIBBREP_SOURCES}" "libbg;libbn;libbu;${OPENNURBS_LIBRARY};${ZLIB_LIBRARY};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}")

set_target_properties(libbrep PROPERTIES VERSION 20.0.1 SOVERSION 20)

BRLCAD_ADDEXEC(test_point_intersect test_point_intersect.cpp "libbrep" NO_STRICT NO_INSTALL)

BRLCAD_ADDEXEC(test_curve_intersect test_curve_intersect.cpp "libbrep" NO_STRICT NO_INSTALL)

add_subdirectory(tests)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
