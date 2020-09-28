set(spsr_DESCRIPTION "
Option for enabling and disabling compilation of the Screened Poisson
Surface Reconstruction library provided with BRL-CAD's source code.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libspsr SPSR libspsr spsr_DESCRIPTION ALIASES ENABLE_SPSR FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_SPSR_BUILD)

  if (MSVC)
    set(SPSR_BASENAME SPSR)
  else (MSVC)
    set(SPSR_BASENAME libSPSR)
  endif (MSVC)

  ExternalProject_Add(SPSR_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libspsr"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(spsr SPSR_BLD
    OUTPUT_FILE ${SPSR_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${SPSR_BASENAME}-static${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(SPSR_BLD ${INCLUDE_DIR}
    SPSR/SPSR.h
    SPSR/cvertex.h
    )

  set(SPSR_LIBRARIES spsr CACHE STRING "Building bundled spsr" FORCE)
  set(SPSR_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/spsr" CACHE STRING "Directory containing spsr headers." FORCE)

  list(APPEND BRLCAD_DEPS SPSR_BLD)

  SetTargetFolder(SPSR_BLD "Third Party Libraries")
  SetTargetFolder(spsr "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_SPSR_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

