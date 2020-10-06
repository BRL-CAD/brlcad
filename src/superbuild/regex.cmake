set (regex_DESCRIPTION "
Option for enabling and disabling compilation of the Regular
Expression Library provided with BRL-CAD's source distribution.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(regex REGEX regex regex_DESCRIPTION ALIASES ENABLE_REGEX)

if (BRLCAD_REGEX_BUILD)

  set(REGEX_VERSION "1.0.4")
  if (MSVC)
    set(REGEX_BASENAME regex_brl)
    set(REGEX_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(REGEX_BASENAME libregex_brl)
    set(REGEX_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${REGEX_VERSION})
  endif (MSVC)

  # Platform differences in default linker behavior make it difficult to
  # guarantee that our libregex symbols will override libc. We'll avoid the
  # issue by renaming our libregex symbols to be incompatible with libc.
  ExternalProject_Add(REGEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/regex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DREGEX_PREFIX_STR=libregex_
    )

  # Tell the parent build about files and libraries
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "
  ExternalProject_Target(regex REGEX_BLD
    OUTPUT_FILE ${REGEX_BASENAME}${REGEX_SUFFIX}
    STATIC_OUTPUT_FILE ${REGEX_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS \"${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1\"
    LINK_TARGET \"${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\"
    STATIC_LINK_TARGET \"${REGEX_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}\"
    RPATH
    )
  ExternalProject_ByProducts(REGEX_BLD ${INCLUDE_DIR}
    regex.h
    )
  \n")

  list(APPEND BRLCAD_DEPS REGEX_BLD)

  set(REGEX_LIBRARIES regex CACHE STRING "Building bundled libregex" FORCE)
  set(REGEX_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

  SetTargetFolder(REGEX_BLD "Third Party Libraries")
  SetTargetFolder(regex "Third Party Libraries")

endif (BRLCAD_REGEX_BUILD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

