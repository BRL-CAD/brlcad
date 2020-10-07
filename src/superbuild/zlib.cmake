set (zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(libz ZLIB zlib zlib_DESCRIPTION ALIASES ENABLE_ZLIB ENABLE_LIBZ)

# TODO - it's not enough just to build - we also need to be able to reset
# if we have already built and the top level options change from enabled
# to disabled.  Resetting entails an unset(var CACHE) on all the find_package
# variables associated with this package, and removing from the build directory
# any previously generated build products.

# In the long run it might be cleaner to compile the 3rd party component with a
# target dir like CMAKE_BUILD_DIR/superbuild/zlib.  Then, define a ZLIB_CPY
# target that copies all the relevant files to the build dir and a ZLIB_CLEAN
# that removes them but doesn't remove superbuild/zlib.  That way flipping
# between bundled and system won't trigger needless rebuilds.

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

  if (NOT DEFINED ZLIB_BLD_DIR)
    set(ZLIB_BLD_DIR "${CMAKE_INSTALL_PREFIX}/superbuild/zlib")
  endif (NOT DEFINED ZLIB_BLD_DIR)

  ExternalProject_Add(ZLIB_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zlib"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${ZLIB_BLD_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZ_PREFIX_STR=${Z_PREFIX_STR}
    )

  # Tell the parent build about files and libraries
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "
  ExternalProject_Target(zlib ZLIB_BLD
    OUTPUT_FILE ${ZLIB_BASENAME}${ZLIB_SUFFIX}
    STATIC_OUTPUT_FILE ${ZLIB_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS \"${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1\"
    LINK_TARGET \"${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\"
    RPATH
    )
  ExternalProject_ByProducts(ZLIB_BLD ${INCLUDE_DIR}
    zconf.h
    zlib.h
    )
  \n")

  # We're supplying ZLIB - tell find_package where to look
  file(APPEND "${SUPERBUILD_OUTPUT}" "set(ZLIB_ROOT \"${CMAKE_INSTALL_PREFIX}\")\n")

  # We need to stage the files from the install directory used by the build
  # (or the supplied pre-compiled dir, if defined) to the BRL-CAD build dirs.
  file(APPEND "${SUPERBUILD_OUTPUT}" "file(GLOB_RECURSE ZLIB_FILES RELATIVE \"${ZLIB_BLD_DIR}\" \"${ZLIB_BLD_DIR}/*\")\n")
  file(APPEND "${SUPERBUILD_OUTPUT}" "foreach(ZF \${ZLIB_FILES})\n")
  file(APPEND "${SUPERBUILD_OUTPUT}" "   execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy \"${ZLIB_BLD_DIR}/\${ZF}\" \"\${BRLCAD_BINARY_DIR}/\${ZF}\")\n")
  file(APPEND "${SUPERBUILD_OUTPUT}" "endforeach(ZF \${ZLIB_FILES})\n")

else (BRLCAD_ZLIB_BUILD)

  # We're NOT supplying ZLIB - clear key outputs from build dir, if present,
  # so they won't interfere with find_package or the build targets
  set(CLEAR_FILES
    ${ZLIB_BASENAME}${ZLIB_SUFFIX}
    ${ZLIB_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1
    zconf.h
    zlib.h
    )
  foreach(CF ${CLEAR_FILES})
    file(REMOVE "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${CF}")
    file(REMOVE "${CMAKE_INSTALL_PREFIX}/${BIN_DIR}/${CF}")
    file(REMOVE "${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR}/${CF}")
    set(ALL_FILES ${LIB_FILES} ${BIN_FILES} ${INC_FILES})
      file(REMOVE "${CMAKE_INSTALL_PREFIX}/${CFG_TYPE}/${LIB_DIR}/${CF}")
      file(REMOVE "${CMAKE_INSTALL_PREFIX}/${CFG_TYPE}/${BIN_DIR}/${CF}")
      file(REMOVE "${CMAKE_INSTALL_PREFIX}/${CFG_TYPE}/${INCLUDE_DIR}/${CF}")
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endforeach(CF ${CLEAR_FILES})

  # If BRLCAD_DEPS is set, set ZLIB_ROOT to prefer that directory
  if (BRLCAD_DEPS)
    if (EXISTS "${BRLCAD_DEPS}/zlib")
      file(APPEND "${SUPERBUILD_OUTPUT}" "set(ZLIB_ROOT \"${BRLCAD_DEPS}/zlib\")\n")
    elseif (EXISTS "${BRLCAD_DEPS}")
      file(APPEND "${SUPERBUILD_OUTPUT}" "set(ZLIB_ROOT \"${BRLCAD_DEPS}\")\n")
    endif (EXISTS "${BRLCAD_DEPS}/zlib")
  endif (BRLCAD_DEPS)

endif (BRLCAD_ZLIB_BUILD)

# In case we have altered settings from one configure stage to the next, we
# need to make sure the find_package variables are cleared.  This way
# find_package will actually run again, and pick up what might be new vales
# compared to the previous run.
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_FOUND CACHE)\n")
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_LIBRARY CACHE)\n")
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_LIBRARIES CACHE)\n")
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_INCLUDE_DIR CACHE)\n")
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_INCLUDE_DIRS CACHE)\n")
file(APPEND "${SUPERBUILD_OUTPUT}" "unset(ZLIB_VERSION_STRING CACHE)\n")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

