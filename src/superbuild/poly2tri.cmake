set(poly2tri_DESCRIPTION "
Option for enabling and disabling compilation of the poly2tri 2D constrained
Delaunay triangulation library provided with BRL-CAD's source code.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing first
for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(poly2tri POLY2TRI poly2tri poly2tri_DESCRIPTION ALIASES ENABLE_POLY2TRI FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_POLY2TRI_BUILD)

  if (MSVC)
    set(POLY2TRI_BASENAME poly2tri)
  else (MSVC)
    set(POLY2TRI_BASENAME libpoly2tri)
  endif (MSVC)

  ExternalProject_Add(POLY2TRI_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/poly2tri"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(poly2tri POLY2TRI_BLD
    OUTPUT_FILE ${POLY2TRI_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${POLY2TRI_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(POLY2TRI_BLD ${INCLUDE_DIR}
    poly2tri/poly2tri.h
    poly2tri/common/shapes.h
    poly2tri/sweep/cdt.h
    poly2tri/sweep/advancing_front.h
    poly2tri/sweep/sweep.h
    poly2tri/sweep/sweep_context.h
    )

  list(APPEND BRLCAD_DEPS POLY2TRI_BLD)

  SetTargetFolder(POLY2TRI_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_POLY2TRI_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

