
### TODO

# Take a look at https://github.com/danielaparker/jsoncons and see if its
# bson extension can do what we need - the jsoncons setup is header only,
# so it might be simpler to integrate and keep updated


# For testing
# set(BRLCAD_ENABLE_BINARY_ATTRIBUTES ON)
set(libbson_DESCRIPTION "
Option for enabling and disabling compilation of the Libbson library
provided with BRL-CAD's source code.  Default is BUNDLED, using
the included other/src version.
")
THIRD_PARTY(bson BSON Libbson
  libbson_DESCRIPTION
  ALIASES ENABLE_BSON
  REQUIRED_VARS BRLCAD_ENABLE_BINARY_ATTRIBUTES
  FLAGS NOSYS UNDOCUMENTED
  RESET_VARS BSON_LIBRARY BSON_INCLUDE_DIR
  )

if (BRLCAD_BSON_BUILD)

  set_lib_vars(BSON bson-1.0 "1" "3" "5")

  ExternalProject_Add(BSON_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bson"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED bson BSON_BLD
    ${BSON_BASENAME}${BSON_SUFFIX}
    SYMLINKS ${BSON_SYMLINK_1};${BSON_SYMLINK_2}
    LINK_TARGET ${BSON_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    RPATH
    )
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/BSON_BLD-prefix")
  if (BUILD_STATIC_LIBS)
    ExternalProject_Target(STATIC bson-static BSON_BLD
      ${BSON_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
  endif (BUILD_STATIC_LIBS)
 
  ExternalProject_ByProducts(bson BSON_BLD ${BSON_INSTDIR} ${INCLUDE_DIR}
    libbson-1.0/bson-endian.h
    libbson-1.0/bson-md5.h
    libbson-1.0/bson-value.h
    libbson-1.0/bson-stdint.h
    libbson-1.0/bson-reader.h
    libbson-1.0/bson-context.h
    libbson-1.0/bson-string.h
    libbson-1.0/bson-error.h
    libbson-1.0/bson-macros.h
    libbson-1.0/bson-compat.h
    libbson-1.0/bson-version.h
    libbson-1.0/bson-oid.h
    libbson-1.0/bson-version-functions.h
    libbson-1.0/bson-iter.h
    libbson-1.0/bson-stdint-win32.h
    libbson-1.0/bson-atomic.h
    libbson-1.0/bson-memory.h
    libbson-1.0/bcon.h
    libbson-1.0/bson-json.h
    libbson-1.0/bson-keys.h
    libbson-1.0/bson-utf8.h
    libbson-1.0/bson-types.h
    libbson-1.0/bson.h
    libbson-1.0/bson-writer.h
    libbson-1.0/bson-config.h
    libbson-1.0/bson-clock.h
    )

  list(APPEND BRLCAD_DEPS BSON_BLD)

  set(BSON_LIBRARIES bson CACHE STRING "Building bundled libbson" FORCE)
  set(BSON_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/${INCLUDE_DIR}" CACHE STRING "Directory containing bson headers." FORCE)

  SetTargetFolder(BSON_BLD "Third Party Libraries")
  SetTargetFolder(bson "Third Party Libraries")

endif (BRLCAD_BSON_BUILD)

include("${CMAKE_CURRENT_SOURCE_DIR}/bson.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

