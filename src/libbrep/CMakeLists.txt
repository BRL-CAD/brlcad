# Include directories needed by libbrep users
set(BREP_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${POLY2TRI_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIRS}
  )

# locally used but not needed by users of the library
set(BREP_LOCAL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REGEX_INCLUDE_DIRS}
  )

BRLCAD_LIB_INCLUDE_DIRS(brep BREP_INCLUDE_DIRS BREP_LOCAL_INCLUDE_DIRS)

set(LIBBREP_SOURCES
  BBNode.cpp
  BRNode.cpp
  PullbackCurve.cpp
  Subcurve.cpp
  Subsurface.cpp
  boolean.cpp
  cdt/cdt.cpp
  cdt/fast.cpp
  cdt/surf.cpp
  cdt/util.cpp
  cdt/validate.cpp
  cdt/edge.cpp
  cdt/mesh.cpp
  cdt/ovlps_simple.cpp
  cdt/tri_isect.cpp
  debug_plot.cpp
  intersect.cpp
  tools/tools.cpp
  opennurbs_ext.cpp
  px_event.cpp
  shape_recognition/shape_recognition.cpp
  shape_recognition/cone.cpp
  shape_recognition/cylinder.cpp
  shape_recognition/pipeline.cpp
  shape_recognition/planar.cpp
  shape_recognition/sphere.cpp
  shape_recognition/util.cpp
  ssx_event.cpp
  tools/util.cpp
  )

BRLCAD_ADDLIB(libbrep "${LIBBREP_SOURCES}" "libbv;libbg;libbn;libbu;${OPENNURBS_LIBRARIES};${POLY2TRI_LIBRARIES};${WINSOCK_LIB};${RPCRT_LIB};${STDCXX_LIBRARIES}")
set_target_properties(libbrep PROPERTIES VERSION 20.0.1 SOVERSION 20)

if(HIDE_INTERNAL_SYMBOLS)
  set_property(TARGET libbrep APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
  # Anyone who needs to import libbrep will also need to import openNURBS
  set_property(TARGET libbrep APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
endif(HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  PullbackCurve.h
  brep_defines.h
  brep_except.h
  cdt/RTree.h
  cdt/cdt.h
  cdt/mesh.h
  debug_plot.h
  shape_recognition/shape_recognition.h
  shape_recognition/torus.cpp
  tools/tools.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
