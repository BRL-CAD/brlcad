set(libgdiam_DESCRIPTION "
Option for enabling and disabling compilation of the libgdiam approximate
tight bounding box library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libgdiam GDIAM libgdiam libgdiam_DESCRIPTION ALIASES ENABLE_GDIAM FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_GDIAM_BUILD)

  if (MSVC)
    set(GDIAM_BASENAME gdiam)
  else (MSVC)
    set(GDIAM_BASENAME libgdiam)
  endif (MSVC)

  ExternalProject_Add(GDIAM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libgdiam"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(gdiam GDIAM_BLD
    OUTPUT_FILE ${GDIAM_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${GDIAM_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(GDIAM_BLD ${INCLUDE_DIR}
    gdiam.hpp
    )

  list(APPEND BRLCAD_DEPS GDIAM_BLD)

  SetTargetFolder(GDIAM_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_GDIAM_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

