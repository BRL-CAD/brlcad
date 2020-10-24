set (zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(libz ZLIB zlib
  zlib_DESCRIPTION
  ALIASES ENABLE_ZLIB ENABLE_LIBZ
  RESET_VARS ZLIB_LIBRARY ZLIB_LIBRARIES ZLIB_INCLUDE_DIR ZLIB_INCLUDE_DIRS ZLIB_LIBRARY_DEBUG ZLIB_LIBRARY_RELEASE
  )

if (BRLCAD_ZLIB_BUILD)

  set(ZLIB_VERSION 1.2.11)

  set(Z_PREFIX_STR "brl_")
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
  set(Z_PREFIX_STR "${Z_PREFIX_STR}" CACHE STRING "prefix for zlib functions" FORCE)

  if (MSVC)
    set(ZLIB_BASENAME z_brl)
    set(ZLIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(ZLIB_BASENAME libz_brl)
    set(ZLIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${ZLIB_VERSION})
  endif (MSVC)

  set(ZLIB_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/zlib)

  ExternalProject_Add(ZLIB_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${ZLIB_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZ_PREFIX_STR=${Z_PREFIX_STR}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/ZLIB_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED zlib ZLIB_BLD ${ZLIB_INSTDIR}
    ${ZLIB_BASENAME}${ZLIB_SUFFIX}
    SYMLINKS "${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_Target(STATIC zlib-static ZLIB_BLD ${ZLIB_INSTDIR}
    ${ZLIB_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    )
 
  ExternalProject_ByProducts(zlib ZLIB_BLD ${ZLIB_INSTDIR} ${INCLUDE_DIR}
    zconf.h
    zlib.h
    )

  set(ZLIB_LIBRARY_DEBUG zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_LIBRARY_RELEASE zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_LIBRARIES zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)
  set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)

  SetTargetFolder(ZLIB_BLD "Third Party Libraries")
  SetTargetFolder(zlib "Third Party Libraries")

else (BRLCAD_ZLIB_BUILD)

  set(Z_PREFIX_STR "" CACHE STRING "clear prefix for zlib functions" FORCE)
  set(Z_PREFIX_STR)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY_RELEASE} CACHE STRING "ZLIB_LIBRARIES" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE STRING "ZLIB include directory" FORCE)

endif (BRLCAD_ZLIB_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/zlib.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

