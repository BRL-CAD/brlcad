# NOTE: we need to have libpng's internal call to find_package looking for zlib
# locate our local copy if we have one.  Defining the ZLIB_ROOT prefix for
# find_package and setting the library file to our custom library name is
# intended to do this (requires CMake 3.12).

set(png_DESCRIPTION "
Option for enabling and disabling compilation of the Portable Network
Graphics library provided with BRL-CAD's source distribution.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")

# We generally don't want the Mac framework libpng...
set(CMAKE_FIND_FRAMEWORK LAST)

THIRD_PARTY(png PNG png
  png_DESCRIPTION
  ALIASES ENABLE_PNG
  RESET_VARS PNG_LIBRARY_DEBUG PNG_LIBRARY_RELEASE
  )

if (BRLCAD_PNG_BUILD)

  VERSIONS("${CMAKE_CURRENT_SOURCE_DIR}/png/png.h" PNG_MAJOR_VERSION PNG_MINOR_VERSION PNG_PATCH_VERSION)

  set(PNG_VERSION_MAJOR ${PNG_MAJOR_VERSION}${PNG_MINOR_VERSION})
  set(PNG_VERSION_MINOR ${PNG_PATCH_VERSION})
  set(PNG_LIB_NAME png_brl${PNG_VERSION_MAJOR})

  # NOTE: when we bump to libpng 1.6.40, they expose a PNG_DEBUG_POSTFIX which
  # we can supply empty at build time.  For the current version we've added a
  # separate variable to achieve the same result.  Without a solution like
  # that, we tend to get 'd' suffixes appearing in library names and playing
  # havoc with file copy logic
  if (MSVC)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_STATICNAME lib${PNG_LIB_NAME}_static)
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_SYMLINK_1 ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_SYMLINK_2 ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR})
  elseif (APPLE)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_STATICNAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX .${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_SYMLINK_1 ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_SYMLINK_2 ${PNG_BASENAME}.${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (OPENBSD)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_STATICNAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_MAJOR_VERSION}.${PNG_MINOR_VERSION})
  else (MSVC)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_STATICNAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0)
    set(PNG_SYMLINK_1 ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(PNG_SYMLINK_2 ${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR})
  endif (MSVC)

  set(PNG_DEPS)
  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
    list(APPEND PNG_DEPS ZLIB_BLD zlib_stage)
  endif (TARGET ZLIB_BLD)

  set(PNG_INSTDIR ${CMAKE_BINARY_INSTALL_ROOT}/png)

  if (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}.lib)
  elseif (OPENBSD)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1.2)
  else (MSVC)
    set(ZLIB_LIBRARY ${CMAKE_BINARY_ROOT}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif (MSVC)

  ExternalProject_Add(PNG_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/png"
    BUILD_ALWAYS ${EXT_BUILD_ALWAYS} ${LOG_OPTS}
    CMAKE_ARGS
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX=ON>
    $<$<BOOL:${ZLIB_TARGET}>:-DZ_PREFIX_STR=${Z_PREFIX_STR}>
    $<$<NOT:$<BOOL:${CMAKE_CONFIGURATION_TYPES}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
    -DCMAKE_INSTALL_PREFIX=${PNG_INSTDIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=${CMAKE_INSTALL_RPATH_USE_LINK_PATH}
    -DCMAKE_SKIP_BUILD_RPATH=${CMAKE_SKIP_BUILD_RPATH}
    -DPNG_LIB_NAME=${PNG_LIB_NAME}
    -DPNG_NO_DEBUG_POSTFIX=ON
    -DPNG_DEBUG_POSTFIX=""
    -DPNG_PREFIX=brl_
    -DPNG_STATIC=${BUILD_STATIC_LIBS}
    -DPNG_TESTS=OFF
    -DSKIP_INSTALL_EXECUTABLES=ON -DSKIP_INSTALL_FILES=ON
    -DSKIP_INSTALL_EXPORT=ON
    -DSKIP_INSTALL_EXPORT=ON
    -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${ZLIB_LIBRARY}>
    -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_ROOT}>
    -Dld-version-script=OFF
    DEPENDS ${PNG_DEPS}
    LOG_CONFIGURE ${EXT_BUILD_QUIET}
    LOG_BUILD ${EXT_BUILD_QUIET}
    LOG_INSTALL ${EXT_BUILD_QUIET}
    LOG_OUTPUT_ON_FAILURE ${EXT_BUILD_QUIET}
    )

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/PNG_BLD-prefix")

  # Tell the parent build about files and libraries
  ExternalProject_Target(SHARED png PNG_BLD ${PNG_INSTDIR}
    ${PNG_BASENAME}${PNG_SUFFIX}
    SYMLINKS ${PNG_SYMLINK_1};${PNG_SYMLINK_2}
    LINK_TARGET ${PNG_SYMLINK_1}
    RPATH
    )
  if (BUILD_STATIC_LIBS)
    ExternalProject_Target(STATIC png-static PNG_BLD ${PNG_INSTDIR}
      ${PNG_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      SYMLINKS ${PNG_STATICNAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
      )
  endif (BUILD_STATIC_LIBS)

  ExternalProject_ByProducts(png PNG_BLD ${PNG_INSTDIR} ${INCLUDE_DIR}
    png.h
    pngconf.h
    pnglibconf.h
    libpng_brl${PNG_VERSION_MAJOR}/png.h
    libpng_brl${PNG_VERSION_MAJOR}/pngconf.h
    libpng_brl${PNG_VERSION_MAJOR}/pnglibconf.h
    )

  set(PNG_LIBRARY_DEBUG png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_LIBRARY_RELEASE png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_LIBRARIES png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_PNG_INCLUDE_DIR "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)
  set(PNG_INCLUDE_DIRS "${CMAKE_BINARY_ROOT}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  SetTargetFolder(PNG_BLD "Third Party Libraries")
  SetTargetFolder(png "Third Party Libraries")

else (BRLCAD_PNG_BUILD)

  set(PNG_INCLUDE_DIRS "${PNG_PNG_INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  if (NOT DEFINED PNG_LIBRARIES)
    if (PNG_LIBRARY_RELEASE)
      set(PNG_LIBRARIES ${PNG_LIBRARY_RELEASE} CACHE STRING "libpng" FORCE)
    else (PNG_LIBRARY_RELEASE)
      set(PNG_LIBRARIES ${PNG_LIBRARY_DEBUG} CACHE STRING "libpng" FORCE)
    endif ()
  endif (NOT DEFINED PNG_LIBRARIES)

endif (BRLCAD_PNG_BUILD)

mark_as_advanced(PNG_PNG_INCLUDE_DIR)
mark_as_advanced(PNG_INCLUDE_DIRS)
mark_as_advanced(PNG_LIBRARIES)
mark_as_advanced(PNG_LIBRARY_DEBUG)
mark_as_advanced(PNG_LIBRARY_RELEASE)

include("${CMAKE_CURRENT_SOURCE_DIR}/png.dist")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

