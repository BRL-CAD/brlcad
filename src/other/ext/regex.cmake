set (regex_DESCRIPTION "
Option for enabling and disabling compilation of the Regular
Expression Library provided with BRL-CAD's source distribution.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(regex REGEX regex
  regex_DESCRIPTION
  ALIASES ENABLE_REGEX
  RESET_VARS REGEX_LIBRARY REGEX_LIBRARIES REGEX_INCLUDE_DIR REGEX_INCLUDE_DIRS
  )

if (BRLCAD_REGEX_BUILD)

  set_lib_vars(REGEX regex_brl "1" "0" "4")

  set(REGEX_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/regex)

  # Platform differences in default linker behavior make it difficult to
  # guarantee that our libregex symbols will override libc. We'll avoid the
  # issue by renaming our libregex symbols to be incompatible with libc.
  ExternalProject_Add(REGEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/regex"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${REGEX_INSTDIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DREGEX_PREFIX_STR=libregex_
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/REGEX_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED regex REGEX_BLD ${REGEX_INSTDIR}
    ${REGEX_BASENAME}${REGEX_SUFFIX}
    SYMLINKS ${REGEX_SYMLINK_1};${REGEX_SYMLINK_2}
    LINK_TARGET ${REGEX_SYMLINK_1}
    RPATH
    )
  if (BUILD_STATIC_LIBS)
    ExternalProject_Target(STATIC regex-static REGEX_BLD ${REGEX_INSTDIR}
      ${REGEX_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      RPATH
      )
  endif (BUILD_STATIC_LIBS)

  ExternalProject_ByProducts(regex REGEX_BLD ${REGEX_INSTDIR} ${INCLUDE_DIR}
    regex.h
    )

  set(REGEX_LIBRARIES regex CACHE STRING "Building bundled libregex" FORCE)
  set(REGEX_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

  SetTargetFolder(REGEX_BLD "Third Party Libraries")
  SetTargetFolder(regex "Third Party Libraries")

endif (BRLCAD_REGEX_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/regex.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

